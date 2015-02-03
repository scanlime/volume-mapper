#pragma once

#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Vbo.h"
#include "cinder/gl/Texture.h"

class PointCloudRenderer
{
public:
    void setup(ci::app::App& app, unsigned width, unsigned height);
    void draw(ci::gl::Texture& depth, ci::gl::Texture& color);

    float mPointSize;

private:
    ci::gl::GlslProg mProg;
    ci::gl::Vbo mVbo;
    ci::gl::Texture mParticleSprite;
    unsigned mNumPoints;
};