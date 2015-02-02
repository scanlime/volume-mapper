#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/Rand.h"
#include "cinder/Color.h"
#include "cinder/params/Params.h"

#include "CinderFreenect.h"
#include "OPCClient.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class VolumeMapperApp : public AppBasic {
public:
	void prepareSettings( Settings* settings );
	void setup();
	void update();
	void draw();
	
private:
    params::InterfaceGlRef  mParams;
    
	KinectRef           mKinect;
    gl::Fbo             mFloatFbo;
    gl::GlslProgRef     mFilterProg;
    gl::TextureRef		mColorTexture, mDepthTexture;

    OPCClient           mOPC;
    vector<char>        mPacket;
    
    int                 mCurrentLed;
    int                 mCurrentFrame;

    int                 mNumLeds;
    int                 mFramesPerLed;
    float               mGain;
    Color               mLedColor;
    
    struct LedFrame {
        gl::TextureRef  rgb;
        gl::TextureRef  depth;
    };

    struct Led {
        gl::Fbo          filter;
        vector<LedFrame> frames;
    };
    
    vector<Led>         mLeds;

    void drawGrid(gl::Texture& tex, int index);
    void updateFilter(Led& led);
};

void VolumeMapperApp::prepareSettings( Settings* settings )
{
    settings->disableFrameRate();
	settings->setWindowSize( 1920, 1080 );
}

void VolumeMapperApp::setup()
{
    gl::disableVerticalSync();

    mKinect = Kinect::create();

    mNumLeds = 16;
    mFramesPerLed = 4;
    mGain = 120.0;
    mCurrentLed = 0;
    mCurrentFrame = 0;
    mLedColor.set(1.0f, 1.0f, 1.0f);

    mFilterProg = gl::GlslProg::create(loadResource("filter.glslv"), loadResource("filter.glslf"));

    mOPC.connectConnectEventHandler(&OPCClient::onConnect, &mOPC);
    mOPC.connectErrorEventHandler(&OPCClient::onError, &mOPC);

    // Static allocation for packet buffer, so we don't resize it during an async write
    mPacket.reserve(sizeof(OPCClient::Header) + 0xFFFF);
    
    mParams = params::InterfaceGl::create( getWindow(), "Mapper parameters", toPixels(Vec2i(240, 400)) );
    mParams->minimize();
    
    mParams->addParam("Number of LEDs", &mNumLeds).min(1).max(0xffff/3);
    mParams->addParam("Frames per LED", &mFramesPerLed);
    mParams->addParam("LED Color", &mLedColor);
    mParams->addParam("Gain", &mGain).min(0.f).max(999.9f).step(0.1f);
}

void VolumeMapperApp::update()
{
    mLeds.resize(mNumLeds);

    if (mKinect->checkNewDepthFrame()) {
        mDepthTexture = gl::Texture::create(mKinect->getDepthImage());
    }
        
    if (mKinect->checkNewVideoFrame()) {
        mColorTexture = gl::Texture::create(mKinect->getVideoImage());

        if (mCurrentLed < mLeds.size()) {
            // Store the frame we just captured

            Led &l = mLeds[mCurrentLed];
            l.frames.resize(max(min<int>( mFramesPerLed, l.frames.size()), mCurrentFrame + 1 ));
            LedFrame &f = l.frames[mCurrentFrame];
            f.rgb = mColorTexture;
            f.depth = mDepthTexture;
            
            updateFilter(l);
        }

        mCurrentFrame++;
        if (mCurrentFrame >= mFramesPerLed) {
            mCurrentFrame = 0;
            mCurrentLed++;
            if (mCurrentLed >= mNumLeds) {
                mCurrentLed = 0;
            }
        }

        auto& header = OPCClient::Header::view(mPacket);
        mPacket.resize(sizeof(OPCClient::Header) + mNumLeds * 3);
        fill(mPacket.begin(), mPacket.end(), 0);
        header.init(0, mOPC.SET_PIXEL_COLORS, mNumLeds * 3);

        // Blink the current LED on alternate frames
        if ((mCurrentFrame & 1) && mCurrentLed < mNumLeds) {
            for (int ch = 0; ch < 3; ch++) {
                header.data()[mCurrentLed*3 + ch] = 255 * mLedColor[ch];
            }
        }
        
        // Write two back-to-back frames, so this takes effect
        // immediately even if Fadecandy's interpolation is enabled.
        mOPC.write(mPacket);
        mOPC.write(mPacket);
        mOPC.update();
    }
}

void VolumeMapperApp::draw()
{
    gl::setViewport(Area(Vec2i(0,0), getWindowSize()));
	gl::setMatricesWindow( getWindowWidth(), getWindowHeight() );
    gl::clear();

    for (int i = 0; i < mLeds.size(); i++) {
        Led& led = mLeds[i];
        if (led.filter) {
            drawGrid(led.filter.getTexture(), i);
        }
    }
    
    mParams->draw();
}

void VolumeMapperApp::drawGrid(gl::Texture& tex, int index)
{
    Vec2i size(320, 240);
    int width = getWindowWidth() / size.x;
    Vec2i pos(size.x * (index % width), size.y * (index / width));
    Rectf rect(pos, size+pos);
    gl::draw(tex, Rectf(pos, pos+size));
}

void VolumeMapperApp::updateFilter(Led& led)
{
    if (led.frames.size() < 1) {
        return;
    }

    if (!led.filter) {
        LedFrame& first = led.frames[0];
        gl::Fbo::Format format;
        format.setColorInternalFormat(GL_RGB32F_ARB);
        led.filter = gl::Fbo(first.rgb->getWidth(), first.rgb->getHeight(), format);
    }
    
    led.filter.bindFramebuffer();
    gl::setViewport(Area(Vec2i(0,0), led.filter.getSize()));
    gl::setMatricesWindow(led.filter.getSize());
    gl::clear();
    gl::enableAdditiveBlending();
    
    mFilterProg->bind();
    mFilterProg->uniform("gain", mGain / led.frames.size());
    mFilterProg->uniform("frame1", 0);
    mFilterProg->uniform("frame2", 1);

    static const float positionData[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLint position = mFilterProg->getAttribLocation("position");
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, &positionData[0]);
    glEnableVertexAttribArray(position);
    
    for (int i = 0; i + 1 < led.frames.size(); i++) {
        led.frames[i].rgb->bind(0);
        led.frames[i+1].rgb->bind(1);
        glDrawArrays(GL_QUADS, 0, 4);
    }

    mFilterProg->unbind();
    gl::disableAlphaBlending();
    glDisableVertexAttribArray(position);
    led.filter.unbindFramebuffer();
}


CINDER_APP_BASIC( VolumeMapperApp, RendererGl )
