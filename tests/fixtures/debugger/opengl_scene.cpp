
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QSurfaceFormat>
#include <cstdio>

struct Vertex {
  float position[3];
  float color[4];
};

struct Scene {
  float model[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
  Vertex vertices[3] = {{{-0.5f, -0.5f, 0}, {1, 0, 0, 1}},
                        {{0.5f, -0.5f, 0}, {0, 1, 0, 1}},
                        {{0, 0.5f, 0}, {0, 0, 1, 1}}};
  GLuint vertexBuffer = 0;
  GLuint texture = 0;
  GLint viewport[4] = {};
};

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QOffscreenSurface surface;
  surface.setFormat(format);
  surface.create();
  QOpenGLContext context;
  context.setFormat(format);
  if (!context.create() || !context.makeCurrent(&surface))
    return 2;
  auto *gl = context.functions();
  gl->initializeOpenGLFunctions();
  Scene scene;
  QOpenGLVertexArrayObject vao;
  if (!vao.create())
    return 4;
  vao.bind();
  gl->glGenBuffers(1, &scene.vertexBuffer);
  gl->glBindBuffer(GL_ARRAY_BUFFER, scene.vertexBuffer);
  gl->glBufferData(GL_ARRAY_BUFFER, sizeof(scene.vertices), scene.vertices,
                   GL_STATIC_DRAW);
  gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
  gl->glEnableVertexAttribArray(0);
  gl->glGenTextures(1, &scene.texture);
  gl->glBindTexture(GL_TEXTURE_2D, scene.texture);
  const unsigned char pixels[16] = {255, 0, 0,   255, 0,   255, 0,   255,
                                    0,   0, 255, 255, 255, 255, 255, 255};
  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, pixels);
  QOpenGLShaderProgram shader;
  if (!shader.addShaderFromSourceCode(
          QOpenGLShader::Vertex,
          "#version 330 core\nlayout(location=0) in vec3 p;"
          "void main(){gl_Position=vec4(p,1.0);}") ||
      !shader.addShaderFromSourceCode(
          QOpenGLShader::Fragment,
          "#version 330 core\nout vec4 c;void main(){c=vec4(1,0,0,1);}") ||
      !shader.link() || !shader.bind())
    return 5;
  QOpenGLFramebufferObject framebuffer(64, 64);
  if (!framebuffer.isValid() || !framebuffer.bind())
    return 6;
  gl->glViewport(0, 0, 64, 64);
  gl->glGetIntegerv(GL_VIEWPORT, scene.viewport);
  gl->glClear(GL_COLOR_BUFFER_BIT);
  gl->glDrawArrays(GL_TRIANGLES, 0, 3);
  gl->glFinish();
  const GLenum graphicsError = gl->glGetError();
  const char *graphicsVersion =
      reinterpret_cast<const char *>(gl->glGetString(GL_VERSION));
  scene.model[0][0] = 2.0f;
  std::printf("opengl-debug-ok %u %u %u %s\n", graphicsError,
              scene.vertexBuffer, scene.texture, graphicsVersion);
  gl->glDeleteBuffers(1, &scene.vertexBuffer);
  gl->glDeleteTextures(1, &scene.texture);
  return graphicsError == GL_NO_ERROR ? 0 : 7;
}
