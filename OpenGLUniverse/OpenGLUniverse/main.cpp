//
//  main.cpp
//  OpenGLUniverse
//
//  Created by lidong on 2020/5/27.
//  Copyright © 2020 李东. All rights reserved.
//


#include "GLTools.h"
#include "GLShaderManager.h"
#include "GLFrustum.h"
#include "GLBatch.h"
#include "GLMatrixStack.h"
#include "GLGeometryTransform.h"
#include "StopWatch.h"

#include <math.h>
#include <stdio.h>

#ifdef __APPLE__
#include <glut/glut.h>
#else
#define FREEGLUT_STATIC
#include <GL/glut.h>
#endif


// 添加附加随机球
#define NUM_SPHERES 50
GLFrame spheres[NUM_SPHERES];

GLShaderManager         shaderManager;          // 着色器管理器
GLMatrixStack           modelViewMatrix;        // 模型视图矩阵
GLMatrixStack           projectionMatrix;       // 投影矩阵
GLFrustum               viewFrusrum;            // 视景体
GLGeometryTransform     transformPipeline;      // 几何图形变化管道
GLTriangleBatch         torusBatch;             // 花托批量处理
GLBatch                 floorBatch;             // 地板批量处理

// 定义公转球的批量处理(公球自转)
GLTriangleBatch         sphereBatch;            // 球批处理

// 照相机角色帧 (全局照相机实例)
GLFrame                 cameraFrame;

// 纹理标记数组
GLuint uiTextures[3];

// 添加纹理
bool loadTGATexture(const char * szFileName, GLenum minFilter, GLenum magFilter, GLenum warpMode) {
    GLbyte * pBits;
    int nWidth, nHeight, nComponents;
    GLenum eFormat;
    
    // 读取纹理数据
    pBits = gltReadTGABits(szFileName, &nWidth, &nHeight, &nComponents, &eFormat);
    if (pBits == NULL) {
        return false;
    }
    
    // 设置纹理参数 参数1: 纹理纬度 参数2: 为S/T坐标设置模式  参数3: wrapMode, 环绕模式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, warpMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, warpMode);
    
    // 参数1: 纹理纬度 参数2: 线性过滤 参数3: 环绕模式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, nWidth, nHeight, 0, eFormat, GL_UNSIGNED_BYTE, pBits);
    
    // 使用完释放pBits
    free(pBits);
    
    if (minFilter == GL_LINEAR_MIPMAP_LINEAR ||
        minFilter == GL_LINEAR_MIPMAP_NEAREST ||
        minFilter == GL_NEAREST_MIPMAP_NEAREST ||
        minFilter == GL_NEAREST_MIPMAP_LINEAR) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    return true;
}

void setupRC() {
    // 设置清屏颜色到颜色缓冲区
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // 初始化着色器管理器
    shaderManager.InitializeStockShaders();
    // 开启深度测试/背面剔除
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    // 设置大球
    gltMakeSphere(torusBatch, 0.4f, 40, 80);
    // 设置小球
    gltMakeSphere(sphereBatch, 0.1f, 26, 13);
    // 设置地板顶点数据&地板纹理
    GLfloat texSize = 10.0f;
    floorBatch.Begin(GL_TRIANGLE_FAN, 4, 1);
    floorBatch.MultiTexCoord2f(0, 0.0f, 0.0f);
    floorBatch.Vertex3f(-20.0f, -0.41f, 20.0f);
    
    floorBatch.MultiTexCoord2f(0, texSize, 0.0f);
    floorBatch.Vertex3f(20.0f, -0.41f, 20.f);
    
    floorBatch.MultiTexCoord2f(0, texSize, texSize);
    floorBatch.Vertex3f(20.0f, -0.41f, -20.0f);
    
    floorBatch.MultiTexCoord2f(0, 0.0f, texSize);
    floorBatch.Vertex3f(-20.0f, -0.41f, -20.0f);
    floorBatch.End();
    
    // 随机小球顶点坐标数据
    for (int i = 0; i < NUM_SPHERES; i++) {
        //y轴不变，X,Z产生随机值
        GLfloat x = ((GLfloat)((rand() % 400) - 200 ) * 0.1f);
        GLfloat z = ((GLfloat)((rand() % 400) - 200 ) * 0.1f);
        
        //在y方向，将球体设置为0.0的位置，这使得它们看起来是飘浮在眼睛的高度
        //对spheres数组中的每一个顶点，设置顶点数据
        spheres[i].SetOrigin(x, 0.0f, z);
    }
    
    // 命名纹理对象
    glGenTextures(3, uiTextures);
    
    // 将TGA文件加载为2D纹理。
    // 参数1：纹理文件名称
    // 参数2&参数3：需要缩小&放大的过滤器
    // 参数4：纹理坐标环绕模式
    glBindTexture(GL_TEXTURE_2D, uiTextures[0]);
    loadTGATexture("marble.tga", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    
    
    glBindTexture(GL_TEXTURE_2D, uiTextures[1]);
    loadTGATexture("marslike.tga", GL_LINEAR_MIPMAP_LINEAR,
                   GL_LINEAR, GL_CLAMP_TO_EDGE);
    
    
    glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
    loadTGATexture("moonlike.tga", GL_LINEAR_MIPMAP_LINEAR,
                   GL_LINEAR, GL_CLAMP_TO_EDGE);
}

// 删除纹理
void shutdownRC (void) {
    glDeleteTextures(3, uiTextures);
}

// 屏幕更改大小或已初始化
void changeSize(int nWidth, int nHeight) {
    // 设置视口
    glViewport(0, 0, nWidth, nHeight);
    // 设置投影方式
    viewFrusrum.SetPerspective(35.0f, float(nWidth) / float(nHeight), 1.0f, 100.0f);
    
    // 将投影矩阵加载到投影矩阵堆栈
    projectionMatrix.LoadMatrix(viewFrusrum.GetProjectionMatrix());
    modelViewMatrix.LoadIdentity();
    
    // 将投影矩阵堆栈和模型视图矩阵对象设置到管道中
    transformPipeline.SetMatrixStacks(modelViewMatrix, projectionMatrix);
}

void drawSomething(GLfloat yRot) {
    //1.定义光源位置&漫反射颜色
    static GLfloat vWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static GLfloat vLightPos[] = { 0.0f, 3.0f, 0.0f, 1.0f };
    
    //2.绘制悬浮小球球
    glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
    for(int i = 0; i < NUM_SPHERES; i++) {
        modelViewMatrix.PushMatrix();
        modelViewMatrix.MultMatrix(spheres[i]);
        shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                     modelViewMatrix.GetMatrix(),
                                     transformPipeline.GetProjectionMatrix(),
                                     vLightPos,
                                     vWhite,
                                     0);
        sphereBatch.Draw();
        modelViewMatrix.PopMatrix();
    }
    
    //3.绘制大球球
    modelViewMatrix.Translate(0.0f, 0.2f, -2.5f);
    modelViewMatrix.PushMatrix();
    modelViewMatrix.Rotate(yRot, 0.0f, 1.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, uiTextures[1]);
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                 modelViewMatrix.GetMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightPos,
                                 vWhite,
                                 0);
    torusBatch.Draw();
    modelViewMatrix.PopMatrix();
    
    //4.绘制公转小球球（公转自转)
    modelViewMatrix.PushMatrix();
    modelViewMatrix.Rotate(yRot * -2.0f, 0.0f, 1.0f, 0.0f);
    modelViewMatrix.Translate(0.8f, 0.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                 modelViewMatrix.GetMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightPos,
                                 vWhite,
                                 0);
    sphereBatch.Draw();
    modelViewMatrix.PopMatrix();
    
}

// 进行调用绘制场景
void renderScene(void) {
    // 地板颜色值
    static GLfloat vFloorColor[] = {1.0f, 1.0f, 0.0f, 0.75f};
    
    // 时间动画
    static CStopWatch rotTimer;
    float yRot = rotTimer.GetElapsedSeconds() * 60.0f;
    
    // 清除颜色缓存区和深度缓存区
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 压栈
    modelViewMatrix.PushMatrix();
    
    // 设置观察者矩阵
    M3DMatrix44f mCamera;
    cameraFrame.GetCameraMatrix(mCamera);
    modelViewMatrix.MultMatrix(mCamera);
    
    // 压栈
    modelViewMatrix.PushMatrix();
    
    // 翻转Y轴
    modelViewMatrix.Scale(1.0f, -1.0f, 1.0f);
    modelViewMatrix.Translate(0.0f, 0.8f, 0.0f);
    
    // 指定顺时针为正面
    glFrontFace(GL_CW);
    
    // 绘制地面以外其他部分
    drawSomething(yRot);
    
    // 恢复逆时针为正面
    glFrontFace(GL_CCW);
    
    // 绘制镜面, 恢复矩阵
    modelViewMatrix.PopMatrix();
    
    // 开启混合功能
    glEnable(GL_BLEND);
    
    // 指定glBlendFunc 颜色混合方程式
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 绑定地面纹理
    glBindTexture(GL_TEXTURE_2D, uiTextures[0]);
    
    // 纹理调整着色器
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_MODULATE,
                                 transformPipeline.GetModelViewProjectionMatrix(),
                                 vFloorColor,
                                 0);
    
    // 开始绘制
    floorBatch.Draw();
    // 取消混合
    glDisable(GL_BLEND);
    // 绘制地面以外其他部分
    drawSomething(yRot);
    // 绘制完 恢复矩阵
    modelViewMatrix.PopMatrix();
    
    // 交换缓存区
    glutSwapBuffers();
    
    // 提交重新渲染
    glutPostRedisplay();
    
}

//**3.移动照相机参考帧，来对方向键作出响应
void SpeacialKeys(int key,int x,int y)
{
    
    float linear = 0.1f;
    float angular = float(m3dDegToRad(5.0f));
    
    if (key == GLUT_KEY_UP) {
        
        //MoveForward 平移
        cameraFrame.MoveForward(linear);
    }
    
    if (key == GLUT_KEY_DOWN) {
        cameraFrame.MoveForward(-linear);
    }
    
    if (key == GLUT_KEY_LEFT) {
        //RotateWorld 旋转
        cameraFrame.RotateWorld(angular, 0.0f, 1.0f, 0.0f);
    }
    
    if (key == GLUT_KEY_RIGHT) {
        cameraFrame.RotateWorld(-angular, 0.0f, 1.0f, 0.0f);
    }
    
    
    
}

int main(int argc, char *argv[]) {
    
    gltSetWorkingDirectory(argv[0]);
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    
    glutCreateWindow("OpenGL SphereWorld");
    
    glutReshapeFunc(changeSize);
    glutDisplayFunc(renderScene);
    glutSpecialFunc(SpeacialKeys);
    
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return 1;
    }
    
    setupRC();
    glutMainLoop();
    shutdownRC();
    
    return 0;
}
