#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
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
	
    params::InterfaceGlRef  mParams;
    
	KinectRef           mKinect;
    gl::TextureRef		mColorTexture, mDepthTexture;
    OPCClient           mOPC;
    vector<char>        mPacket;

    int                 mCurrentLed;
    int                 mColorRotation;
    int                 mReturnIndex[3];        // Where results go, by channel

    int                 mNumLeds;
    int                 mSamplesPerLed;
    float               mGain;
    Color               mLedColor;
    
    struct LedSample {
        gl::TextureRef  rgb;
        gl::TextureRef  depth;
        int channel;
    };

    struct Led {
        vector<LedSample> samples;
    };
    
    vector<Led>         mLeds;
};

void VolumeMapperApp::prepareSettings( Settings* settings )
{
    settings->disableFrameRate();
	settings->setWindowSize( 160*8, 120*8 );
}

void VolumeMapperApp::setup()
{
    gl::disableVerticalSync();

    mKinect = Kinect::create();

    mNumLeds = 64;
    mSamplesPerLed = 15;
    mGain = 4.0;
    mCurrentLed = 0;
    mColorRotation = 0;
    mLedColor.set(1.0f, 1.0f, 0.5f);
    
    for (int ch = 0; ch < 3; ch++) {
        mReturnIndex[ch] = -1;
    }
    
    mPacket.resize(sizeof(OPCClient::Header) + mNumLeds * 3);

    mOPC.connectConnectEventHandler(&OPCClient::onConnect, &mOPC);
    mOPC.connectErrorEventHandler(&OPCClient::onError, &mOPC);
    
    mParams = params::InterfaceGl::create( getWindow(), "Mapper parameters", toPixels(Vec2i(240, 400)) );

    mParams->addParam("Number of LEDs", &mNumLeds).min(1).max(0xffff/3);
    mParams->addParam("Samples per LED", &mSamplesPerLed);
    mParams->addParam("LED Color", &mLedColor);
    mParams->addParam("Gain", &mGain).min(0.f).max(99.9f).step(0.1f);
}

void VolumeMapperApp::update()
{
    mLeds.resize(mNumLeds);

    if (mKinect->checkNewDepthFrame()) {
        mDepthTexture = gl::Texture::create(mKinect->getDepthImage());
    }
        
    if (mKinect->checkNewVideoFrame()) {
        mColorTexture = gl::Texture::create(mKinect->getVideoImage());

        // Store a new sample for each channel
        for (int ch = 0; ch < 3; ch++) {
            int led = mReturnIndex[ch];
            if (led >= 0 && led < mNumLeds) {
                vector<LedSample>& samples = mLeds[led].samples;
                samples.emplace_back();
                samples.back().rgb = mColorTexture;
                samples.back().depth = mDepthTexture;
                samples.back().channel = ch;
                while (samples.size() > mSamplesPerLed) {
                    samples.erase(samples.begin());
                }
            }
        }
        
        // Next LED
        mCurrentLed = Rand::randInt(mNumLeds);
        mColorRotation++;

        auto& header = OPCClient::Header::view(mPacket);
        fill(mPacket.begin(), mPacket.end(), 0);
        header.init(0, mOPC.SET_PIXEL_COLORS, mNumLeds * 3);

        // Light up one channel on one pixel
        int channel = mColorRotation % 3;
        header.data()[mCurrentLed*3 + channel] = 255 * mLedColor[channel];

        // Remember what to do with this data when we see it back on the camera
        mReturnIndex[channel] = mCurrentLed;

        mOPC.write(mPacket);
        mOPC.write(mPacket);
        mOPC.update();
    }
}

void VolumeMapperApp::draw()
{
	gl::setMatricesWindow( getWindowWidth(), getWindowHeight() );
    gl::clear();

    for (int led = 0; led < mLeds.size(); led++) {
        Vec2i size(160, 120);
        Vec2i pos(size.x * (led%8), size.y * (led/8));
        Rectf rect(pos, size+pos);
        
        Led& l = mLeds[led];
        gl::enableAdditiveBlending();
        float c = mGain / l.samples.size();
        for (int i = 0; i < l.samples.size(); i++) {
            gl::color(l.samples[i].channel == 0 ? c : 0.0f,
                      l.samples[i].channel == 1 ? c : 0.0f,
                      l.samples[i].channel == 2 ? c : 0.0f);
            gl::draw(l.samples[i].rgb, Rectf(pos, pos+size));
        }
    }
        
    mParams->draw();
}

CINDER_APP_BASIC( VolumeMapperApp, RendererGl )
