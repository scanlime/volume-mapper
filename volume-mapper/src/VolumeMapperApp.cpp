#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/Color.h"
#include "cinder/MayaCamUI.h"
#include "cinder/params/Params.h"

#include "CinderFreenect.h"
#include "PointCloudRenderer.h"
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
	
    void mouseDown(MouseEvent event);
    void mouseDrag(MouseEvent event);
    void captureBackground();
    
private:
    params::InterfaceGlRef  mParams;
    ci::MayaCamUI           mMayaCam;
    
	KinectRef           mKinect;
    PointCloudRenderer  mPointCloud;
    gl::GlslProgRef     mFilterProg;
    gl::GlslProgRef     mMaskProg;
    gl::TextureRef		mColorTexture;
    gl::TextureRef      mDepthTexture;
    gl::TextureRef      mDepthBackgroundTexture;
    
    OPCClient           mOPC;
    vector<char>        mPacket;
    
    int                 mCurrentLed;
    int                 mCurrentFrame;
    int                 mLastUpdatedLed;
    
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
        gl::Fbo          mask;
        vector<LedFrame> frames;
    };
    
    vector<Led>         mLeds;

    void updateFilter(Led& led);
    void updateDepthMask(Led& led);
};

void VolumeMapperApp::prepareSettings( Settings* settings )
{
    settings->disableFrameRate();
	settings->setWindowSize( 1920, 1080 );
}

void VolumeMapperApp::setup()
{
    gl::disableVerticalSync();

    Kinect::FreenectParams kinectConfig;
    kinectConfig.mDepthRegister = true;
    mKinect = Kinect::create(kinectConfig);
    mPointCloud.setup(*this, 640, 480);

    CameraPersp cam;
    cam.setEyePoint(Vec3f(0.0, 0.0, -0.25));
    cam.setCenterOfInterestPoint(Vec3f(0,0,0));
    cam.setPerspective(60.0f, getWindowAspectRatio(), 0.1, 1000);
    mMayaCam.setCurrentCam(cam);

    gl::disableVerticalSync();
    gl::disable(GL_DEPTH_TEST);
    gl::disable(GL_CULL_FACE);

    mNumLeds = 4;
    mFramesPerLed = 4;
    mGain = 120.0;
    mCurrentLed = 0;
    mCurrentFrame = 0;
    mLastUpdatedLed = 0;
    mLedColor.set(1.0f, 1.0f, 1.0f);

    mFilterProg = gl::GlslProg::create(loadResource("filter.glslv"), loadResource("filter.glslf"));
    mMaskProg = gl::GlslProg::create(loadResource("depthMask.glslv"), loadResource("depthMask.glslf"));

    mOPC.connectConnectEventHandler(&OPCClient::onConnect, &mOPC);
    mOPC.connectErrorEventHandler(&OPCClient::onError, &mOPC);

    // Static allocation for packet buffer, so we don't resize it during an async write
    mPacket.reserve(sizeof(OPCClient::Header) + 0xFFFF);
    
    mParams = params::InterfaceGl::create( getWindow(), "Mapper parameters", toPixels(Vec2i(240, 400)) );
    
    mParams->addParam("Number of LEDs", &mNumLeds).min(1).max(0xffff/3);
    mParams->addParam("Frames per LED", &mFramesPerLed);
    mParams->addParam("LED Color", &mLedColor);
    mParams->addParam("Point size", &mPointCloud.mPointSize).min(0.f).max(50.f).step(0.1f);
    mParams->addParam("Gain", &mGain).min(0.f).max(999.9f).step(0.1f);
    
    mParams->addSeparator();
    mParams->addButton("Capture background", bind(&VolumeMapperApp::captureBackground, this), "key=b");
}

void VolumeMapperApp::captureBackground()
{
    mDepthBackgroundTexture = mDepthTexture;
}

void VolumeMapperApp::update()
{
    mLeds.resize(mNumLeds);

    if (mKinect->checkNewDepthFrame()) {
        mDepthTexture = gl::Texture::create(mKinect->getDepthImage());
        if (!mDepthBackgroundTexture) {
            mDepthBackgroundTexture = mDepthTexture;
        }
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
            updateDepthMask(l);
            mLastUpdatedLed = mCurrentLed;
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
    gl::setMatrices(mMayaCam.getCamera());
    gl::clear();

    if (mColorTexture && mLastUpdatedLed < mLeds.size()) {
        mPointCloud.draw(mLeds[mLastUpdatedLed].mask.getTexture(), *mColorTexture);
    }
    
    mParams->draw();
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

void VolumeMapperApp::updateDepthMask(Led& led)
{
    if (!led.mask) {
        gl::Fbo::Format format;
        format.setColorInternalFormat(GL_RGB32F_ARB);
        led.mask = gl::Fbo(mDepthTexture->getWidth(), mDepthTexture->getHeight(), format);
    }
    
    led.mask.bindFramebuffer();
    gl::setViewport(Area(Vec2i(0,0), led.mask.getSize()));
    gl::setMatricesWindow(led.mask.getSize());
    gl::clear();
    gl::disableAlphaBlending();
    
    mMaskProg->bind();
    mMaskProg->uniform("depth", 0);
    mMaskProg->uniform("background", 1);
    mDepthTexture->bind(0);
    mDepthBackgroundTexture->bind(1);
    
    static const float positionData[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLint position = mMaskProg->getAttribLocation("position");
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, &positionData[0]);
    glEnableVertexAttribArray(position);
    glDrawArrays(GL_QUADS, 0, 4);
    
    mMaskProg->unbind();
    glDisableVertexAttribArray(position);
    led.mask.unbindFramebuffer();
}

void VolumeMapperApp::mouseDown(MouseEvent event)
{
    mMayaCam.mouseDown(event.getPos());
}

void VolumeMapperApp::mouseDrag(MouseEvent event)
{
    mMayaCam.mouseDrag(event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown());
}

CINDER_APP_BASIC( VolumeMapperApp, RendererGl )
