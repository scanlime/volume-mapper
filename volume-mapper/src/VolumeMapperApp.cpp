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
    void clearGrid();
    
private:
    params::InterfaceGlRef  mParams;
    ci::MayaCamUI           mMayaCam;
    
	KinectRef           mKinect;
    PointCloudRenderer  mPointCloud;
    
    gl::GlslProgRef     mFilterProg;
    gl::GlslProgRef     mMaskProg;
    gl::GlslProgRef     mSliceProg;
    gl::GlslProgRef     mDrawGridProg;

    gl::TextureRef		mColorTexture;
    gl::TextureRef      mDepthTexture;
    gl::TextureRef      mDepthBackgroundTexture;
    
    OPCClient           mOPC;
    vector<char>        mPacket;
    
    int                 mCurrentLed;
    int                 mCurrentFrame;
    int                 mBackgroundInitCountdown;

    int                 mNumLeds;
    int                 mFramesPerLed;
    int                 mGridX;
    int                 mGridY;
    int                 mGridZ;
    float               mZLimit;
    float               mGain;
    float               mSliceAlpha;
    Color               mLedColor;

    bool                mViewCameraPointCloud;
    bool                mViewFilteredPointCloud;
    bool                mViewVolumeGrid;
    
    struct Led {
        gl::Fbo                 filter;    // Filtered color buffer, for current depth
        gl::Fbo                 mask;      // Masked depth buffer
        vector<gl::TextureRef>  frames;    // Refs to original frames that we filter
        vector<gl::Fbo>         grid;      // One VBO for each Z slice
    };
    
    vector<Led>         mLeds;

    void updateFilter(Led& led);
    void updateDepthMask(Led& led);
    void updateGrid(Led& led);
    void drawGrid(Led& led);
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
    cam.setEyePoint(Vec3f(0.0, 0.0, -0.33));
    cam.setCenterOfInterestPoint(Vec3f(0,0,0));
    cam.setPerspective(60.0f, getWindowAspectRatio(), 0.1, 1000);
    mMayaCam.setCurrentCam(cam);

    gl::disableVerticalSync();
    gl::disable(GL_DEPTH_TEST);
    gl::disable(GL_CULL_FACE);

    mNumLeds = 16;
    mFramesPerLed = 4;
    mGridX = 128;
    mGridY = 128;
    mGridZ = 128;
    mZLimit = 0.067;

    mSliceAlpha = 0.5;
    mGain = 10.0;
    mCurrentLed = 0;
    mCurrentFrame = 0;
    mViewCameraPointCloud = true;
    mViewFilteredPointCloud = true;
    mViewVolumeGrid = true;
    mLedColor.set(1.0f, 1.0f, 1.0f);

    // Give the system time to stabilize before we latch onto an initial background image
    mBackgroundInitCountdown = 120;
    
    mFilterProg = gl::GlslProg::create(loadResource("filter.glslv"), loadResource("filter.glslf"));
    mSliceProg = gl::GlslProg::create(loadResource("slice.glslv"), loadResource("slice.glslf"));
    mMaskProg = gl::GlslProg::create(loadResource("depthMask.glslv"), loadResource("depthMask.glslf"));
    mDrawGridProg = gl::GlslProg::create(loadResource("drawGrid.glslv"), loadResource("drawGrid.glslf"));

    mOPC.connectConnectEventHandler(&OPCClient::onConnect, &mOPC);
    mOPC.connectErrorEventHandler(&OPCClient::onError, &mOPC);

    // Static allocation for packet buffer, so we don't resize it during an async write
    mPacket.reserve(sizeof(OPCClient::Header) + 0xFFFF);
    
    mParams = params::InterfaceGl::create( getWindow(), "Mapper parameters", toPixels(Vec2i(300, 400)) );
    
    mParams->addParam("Number of LEDs", &mNumLeds).min(1).max(0xffff/3);
    mParams->addButton("Capture background", bind(&VolumeMapperApp::captureBackground, this), "key=b");
    mParams->addButton("Clear grid", bind(&VolumeMapperApp::clearGrid, this), "key=c");
    mParams->addParam("View camera point cloud", &mViewCameraPointCloud, "key=1");
    mParams->addParam("View filtered point cloud", &mViewFilteredPointCloud, "key=2");
    mParams->addParam("View volume grid", &mViewVolumeGrid, "key=3");
    mParams->addSeparator();
    mParams->addParam("Grid size (X)", &mGridX).min(1).max(640);
    mParams->addParam("Grid size (Y)", &mGridY).min(1).max(480);
    mParams->addParam("Grid size (Z)", &mGridZ).min(1).max(1024);
    mParams->addParam("Z Limit", &mZLimit).min(0.001f).max(1.f).step(0.001f);
    mParams->addParam("Frames per LED", &mFramesPerLed);
    mParams->addParam("LED Color", &mLedColor);
    mParams->addParam("Point size", &mPointCloud.mPointSize).min(0.f).max(50.f).step(0.1f);
    mParams->addParam("Gain", &mGain).min(0.f).max(999.9f).step(0.1f);
    mParams->addParam("Slice alpha", &mSliceAlpha).min(0.f).max(1.0f).step(0.01f);
}

void VolumeMapperApp::captureBackground()
{
    mDepthBackgroundTexture = mDepthTexture;
}

void VolumeMapperApp::clearGrid()
{
    for (int i = 0; i < mLeds.size(); i++) {
        mLeds[i].grid.clear();
    }
}

void VolumeMapperApp::update()
{
    mLeds.resize(mNumLeds);
    if (mCurrentLed >= mNumLeds) {
        mCurrentLed = 0;
    }

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
            l.frames[mCurrentFrame] = mColorTexture;
        }

        mCurrentFrame++;
        if (mCurrentFrame >= mFramesPerLed) {

            Led &l = mLeds[mCurrentLed];
            updateDepthMask(l);
            updateFilter(l);
            updateGrid(l);
            
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
    
    if (mBackgroundInitCountdown) {
        mBackgroundInitCountdown--;
        captureBackground();
    }
}

void VolumeMapperApp::draw()
{
    gl::setViewport(Area(Vec2i(0,0), getWindowSize()));
    gl::setMatrices(mMayaCam.getCamera());
    gl::clear();

    // Common coordinate system is the unit cube from [0,0,0] to [1,1,1], in camera space
    gl::translate(0.5, 0.5, 0.2);
    gl::scale(-1.0f, -1.0f, 16.0);
    
    // Draw a cube around the coordinate system bounds
    gl::color(0.2f, 0.2f, 0.8f);
    gl::drawStrokedCube(Vec3f(0.5f, 0.5f, mZLimit / 2.0f), Vec3f(1.0f, 1.0f, mZLimit));
    
    Led& currentLed = mLeds[mCurrentLed];
    
    if (mViewFilteredPointCloud && currentLed.filter && currentLed.mask) {
        mPointCloud.mGain = 1e2 * mGain;
        mPointCloud.draw(currentLed.mask.getTexture(), currentLed.filter.getTexture());
    }

    if (mViewCameraPointCloud && mDepthTexture && mColorTexture) {
        mPointCloud.mGain = 1.0f;
        mPointCloud.draw(*mDepthTexture, *mColorTexture);
    }

    if (mViewVolumeGrid) {
        drawGrid(currentLed);
    }
        
    mParams->draw();
}

void VolumeMapperApp::drawGrid(Led& led)
{
    gl::enableAdditiveBlending();
    mDrawGridProg->bind();
    mDrawGridProg->uniform("gain", mGain);
    mDrawGridProg->uniform("layer", 0);

    static const float positionData[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLint position = mDrawGridProg->getAttribLocation("position");
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, &positionData[0]);
    glEnableVertexAttribArray(position);

    for (int z = 0; z < mGridZ; z++) {
        if (led.grid.size() > z && led.grid[z]) {
            mDrawGridProg->uniform("z", z * mZLimit / float(mGridZ - 1));
            led.grid[z].getTexture().bind(0);
            glDrawArrays(GL_QUADS, 0, 4);
        }
    }
    
    mDrawGridProg->unbind();
    gl::disableAlphaBlending();
    glDisableVertexAttribArray(position);
}

void VolumeMapperApp::updateGrid(Led& led)
{
    float zStep = mZLimit / float(mGridZ - 1);

    led.grid.resize(mGridZ);

    // Don't filter depth values
    led.mask.getTexture().bind();
    led.mask.getTexture().setMinFilter(GL_NEAREST);
    led.mask.getTexture().setMagFilter(GL_NEAREST);

    for (int z = 0; z < mGridZ; z++) {
        // Each slice is built up on a separate FBO
        gl::Fbo& slice = led.grid[z];
        bool needClear = false;

        if (!slice) {
            gl::Fbo::Format format;
            format.setColorInternalFormat(GL_R32F);
            slice = gl::Fbo(mGridX, mGridY, format);
            needClear = true;
        }
 
        slice.bindFramebuffer();
        gl::setViewport(Area(Vec2i(0,0), slice.getSize()));
        gl::setMatricesWindow(slice.getSize());
        if (needClear) {
            gl::clear();
        }
        gl::enableAlphaBlending();
        
        mSliceProg->bind();
        mSliceProg->uniform("z_min", 1e-3f + z * zStep);
        mSliceProg->uniform("z_max", 1e-3f + (z+1) * zStep);
        mSliceProg->uniform("alpha", mSliceAlpha);
        
        mFilterProg->uniform("mask", 0);
        mFilterProg->uniform("filter", 1);
        
        led.mask.getTexture().bind(0);
        led.filter.getTexture().bind(1);

        static const float positionData[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        GLint position = mSliceProg->getAttribLocation("position");
        glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, &positionData[0]);
        glEnableVertexAttribArray(position);
        glDrawArrays(GL_QUADS, 0, 4);
        mSliceProg->unbind();
        gl::disableAlphaBlending();
        glDisableVertexAttribArray(position);

        slice.unbindFramebuffer();
    }
}

void VolumeMapperApp::updateFilter(Led& led)
{
    if (led.frames.size() < 1) {
        return;
    }

    if (!led.filter) {
        gl::Fbo::Format format;
        format.setColorInternalFormat(GL_R32F);
        led.filter = gl::Fbo(led.frames[0]->getWidth(), led.frames[0]->getHeight(), format);
    }
    
    led.filter.bindFramebuffer();
    gl::setViewport(Area(Vec2i(0,0), led.filter.getSize()));
    gl::setMatricesWindow(led.filter.getSize());
    gl::clear();
    gl::enableAdditiveBlending();
    
    mFilterProg->bind();
    mFilterProg->uniform("gain", 1.0f / led.frames.size());
    mFilterProg->uniform("frame1", 0);
    mFilterProg->uniform("frame2", 1);
    mFilterProg->uniform("mask", 2);

    led.mask.getTexture().bind(2);
    
    static const float positionData[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLint position = mFilterProg->getAttribLocation("position");
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, &positionData[0]);
    glEnableVertexAttribArray(position);
    
    for (int i = 0; i + 1 < led.frames.size(); i++) {
        led.frames[i]->bind(0);
        led.frames[i+1]->bind(1);
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
        format.setColorInternalFormat(GL_R32F);
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
