#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"

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
	
	KinectRef		mKinect;
	gl::Texture		mColorTexture, mDepthTexture;	
    OPCClient       mOPC;
    vector<char>    mPacket;
    int             mState;
    int             mNumLeds;
};

void VolumeMapperApp::prepareSettings( Settings* settings )
{
	settings->setWindowSize( 1280, 480 );
}

void VolumeMapperApp::setup()
{
	mKinect = Kinect::create();
    mState = 0;
    mNumLeds = 64;

    mPacket.resize(sizeof(OPCClient::Header) + mNumLeds * 3);

}

void VolumeMapperApp::update()
{
    if( mKinect->checkNewDepthFrame() )
		mDepthTexture = mKinect->getDepthImage();
	
    if( mKinect->checkNewVideoFrame() ) {
		mColorTexture = mKinect->getVideoImage();
    
        // Next LED
        mState++;
        int led = mState % mNumLeds;

        auto header = OPCClient::Header::view(mPacket);
        header.init(0, mOPC.SET_PIXEL_COLORS, mNumLeds * 3);
        for (int ch = 0; ch < 3; ch++) {
            header.data()[led*3 + ch] = 255;
        }
        mOPC.write(mPacket);
    }
}

void VolumeMapperApp::draw()
{
	gl::clear(); 
	gl::setMatricesWindow( getWindowWidth(), getWindowHeight() );
	if( mDepthTexture )
		gl::draw( mDepthTexture );
	if( mColorTexture )
		gl::draw( mColorTexture, Vec2i( 640, 0 ) );
}

CINDER_APP_BASIC( VolumeMapperApp, RendererGl )
