#include "PointCloudRenderer.h"

using namespace std;
using namespace ci;


void PointCloudRenderer::setup(app::App &app, unsigned width, unsigned height)
{
    mProg = gl::GlslProg(app.loadResource("pointCloud.glslv"), app.loadResource("pointCloud.glslf"));
    mVbo = gl::Vbo(GL_ARRAY_BUFFER);
    mParticleSprite = loadImage(app.loadResource("particle.png"));
    
    vector<Vec2f> vertices;
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            vertices.push_back(Vec2f(x / float(width-1), y / float(height-1)));
        }
    }

    mVbo.bufferData(vertices.size() * sizeof(vertices[0]), &vertices[0], GL_STATIC_DRAW);
    mNumPoints = vertices.size();
    mPointSize = 1.0;
}

void PointCloudRenderer::draw(ci::gl::Texture& depth, ci::gl::Texture& color)
{
    mVbo.bind();
    mProg.bind();

    GLint position = mProg.getAttribLocation("position");
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position);
    gl::enableAdditiveBlending();
    gl::enable(GL_POINT_SPRITE);
    gl::enable(GL_PROGRAM_POINT_SIZE_EXT);
    
    mProg.uniform("point_size", 1e-3f * mPointSize * gl::getViewport().getHeight());
    mProg.uniform("depth", 0);
    mProg.uniform("color", 1);
    mProg.uniform("sprite", 2);

    depth.bind(0);
    depth.setMinFilter(GL_NEAREST);
    depth.setMagFilter(GL_NEAREST);
    
    color.bind(1);
    mParticleSprite.bind(2);

    glDrawArrays(GL_POINTS, 0, mNumPoints);
    
    gl::disableAlphaBlending();
    glDisableVertexAttribArray(position);
    gl::disable(GL_POINT_SPRITE);
    gl::disable(GL_PROGRAM_POINT_SIZE_EXT);

    mVbo.unbind();
    mProg.unbind();
}
