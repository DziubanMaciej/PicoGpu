#include "gpu/gpu.h"
#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/conversions.h"
#include "gpu/util/vcd_trace.h"

#include <GL/gl.h>
#include <GL/glut.h>
#include <map>
#include <memory>
#include <third_party/stb_image_write.h>

class GpuWrapper {
public:
    GpuWrapper(Gpu &gpu, sc_clock &clock, size_t screenWidth, size_t screenHeight)
        : gpu(gpu),
          clock(clock),
          screenWidth(screenWidth),
          screenHeight(screenHeight),
          pixels(std::make_unique<uint32_t[]>(screenWidth * screenHeight)) {
        sc_report_handler::set_actions(SC_INFO, SC_DO_NOTHING);

        gpu.blocks.GLOBAL.inpClock(clock);

        addresses.frameBuffer = allocateDwords(screenWidth * screenHeight, "frame buffer");
        gpu.blocks.RS_OM.framebufferWidth.write(screenWidth);
        gpu.blocks.RS_OM.framebufferHeight.write(screenHeight);
        gpu.blocks.OM.inpFramebufferAddress = addresses.frameBuffer;
    }

    void setDepth() {
        addresses.depthBuffer = allocateDwords(screenWidth * screenHeight, "depth buffer");
        gpu.blocks.OM.inpDepthEnable = 1;
        gpu.blocks.OM.inpDepthBufferAddress = addresses.depthBuffer.value();
    }

    void setVertices(uint32_t *vertices, uint32_t verticesCount, uint32_t vertexSizeInDwords) {
        addresses.vb = allocateDwords(verticesCount * vertexSizeInDwords, "vertex buffer");
        uploadData(addresses.vb, vertices, verticesCount * vertexSizeInDwords);
        gpu.blocks.PA.inpVerticesAddress = addresses.vb;
        gpu.blocks.PA.inpVerticesCount = verticesCount;
    }

    void setShaders(const char *vsCode, const char *fsCode) {
        vs.reset();
        fs.reset();

        FATAL_ERROR_IF(Isa::assembly(vsCode, &vs), "Failed to assemble VS");
        FATAL_ERROR_IF(Isa::assembly(fsCode, &fs), "Failed to assemble FS");
        FATAL_ERROR_IF(!Isa::PicoGpuBinary::areShadersCompatible(vs, fs), "VS is not compatible with FS");

        gpu.blocks.GLOBAL.inpVsCustomInputComponents = vs.getVsCustomInputComponents().raw;
        gpu.blocks.GLOBAL.inpVsPsCustomComponents = vs.getVsPsCustomComponents().raw;

        addresses.vs = allocateDwords(vs.getSizeInDwords(), "vertex shader");
        uploadData(addresses.vs, vs.getData().data(), vs.getSizeInDwords());
        gpu.blocks.VS.inpShaderAddress = addresses.vs;
        gpu.blocks.VS.inpUniforms = vs.getUniforms().raw;

        addresses.fs = allocateDwords(fs.getSizeInDwords(), "fragment shader");
        uploadData(addresses.fs, fs.getData().data(), fs.getSizeInDwords());
        gpu.blocks.FS.inpShaderAddress = addresses.fs;
        gpu.blocks.FS.inpUniforms = fs.getUniforms().raw;
    }

    void clearFrameBuffer(uint32_t *color) {
        gpu.commandStreamer.fillMemory(addresses.frameBuffer, color, screenWidth * screenHeight, nullptr);
    }

    void clearDepthBuffer() {
        if (addresses.depthBuffer.has_value()) {
            gpu.commandStreamer.fillMemory(addresses.depthBuffer.value(), reinterpret_cast<uint32_t *>(&infinity), screenWidth * screenHeight, nullptr);
        }
    }

    void setVsUniform(size_t index, uint32_t x = 0, uint32_t y = 0, uint32_t z = 0, uint32_t w = 0) {
        gpu.blocks.VS.inpUniformsData[index][0] = x;
        gpu.blocks.VS.inpUniformsData[index][1] = y;
        gpu.blocks.VS.inpUniformsData[index][2] = z;
        gpu.blocks.VS.inpUniformsData[index][3] = w;
    }

    void setFsUniform(size_t index, uint32_t x = 0, uint32_t y = 0, uint32_t z = 0, uint32_t w = 0) {
        gpu.blocks.FS.inpUniformsData[index][0] = x;
        gpu.blocks.FS.inpUniformsData[index][1] = y;
        gpu.blocks.FS.inpUniformsData[index][2] = z;
        gpu.blocks.FS.inpUniformsData[index][3] = w;
    }

    void draw() {
        gpu.commandStreamer.draw(nullptr);
    }

    void waitForIdle() {
        gpu.waitForIdle(clock);
    }

    const uint32_t *readFramebuffer(bool blocking) {
        readFramebuffer(this->pixels.get(), blocking);
        return this->pixels.get();
    }

    void readFramebuffer(uint32_t *outData, bool blocking) {
        gpu.commandStreamer.blitFromMemory(addresses.frameBuffer, outData, screenWidth * screenHeight, nullptr);
        if (blocking) {
            waitForIdle();
        }
    }

    void summarizeMemory() {
        printf("Memory allocations:\n");
        for (const auto &alloc : memoryAllocations) {
            printf("  %16s: 0x%08x - 0x%08x\n", alloc.label.c_str(), alloc.start, alloc.end);
        }
    }

private:
    void uploadData(MemoryAddressType memoryPtr, uint32_t *userPtr, size_t sizeInDwords) {
        gpu.commandStreamer.blitToMemory(memoryPtr, userPtr, sizeInDwords, nullptr);
    }

    uint32_t allocateDwords(uint32_t dwordsCount, const std::string &label) {
        if (memoryUsed + dwordsCount >= 4 * Gpu::memorySize) {
            summarizeMemory();
            FATAL_ERROR("Out of memory");
        }
        const uint32_t result = memoryUsed;
        memoryAllocations.push_back(MemoryAllocation{result, memoryUsed, label});
        memoryUsed += 4 * dwordsCount;
        return result;
    }

    // Constants
    float infinity = 100;
    Gpu &gpu;
    sc_clock &clock;
    size_t screenWidth;
    size_t screenHeight;

    // Allocated addresses
    struct {
        uint32_t frameBuffer;
        std::optional<uint32_t> depthBuffer;
        uint32_t vs;
        uint32_t fs;
        uint32_t vb;
    } addresses;

    // Shaders
    Isa::PicoGpuBinary vs = {};
    Isa::PicoGpuBinary fs = {};

    // Allocated memory
    uint32_t memoryUsed = 0;
    struct MemoryAllocation {
        uint32_t start;
        uint32_t end;
        std::string label;
    };
    std::vector<MemoryAllocation> memoryAllocations = {};

    // Readback memory
    std::unique_ptr<uint32_t[]> pixels;
};

class WindowWrapper {
public:
    using UpdateFunction = void (*)(GpuWrapper &gpuWrapper);
    WindowWrapper(GpuWrapper &gpuWrapper, UpdateFunction updateFunction, int contentWidth, int contentHeight)
        : gpuWrapper(gpuWrapper),
          updateFunction(updateFunction),
          contentWidth(contentWidth),
          contentHeight(contentHeight) {
        instance = this;

        int argc = 0;
        glutInit(&argc, nullptr);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_ALPHA);
        glutCreateWindow(TEST_NAME);
        glutDisplayFunc(handleDisplay);
        glutKeyboardFunc(handleKeyboard);
        glutReshapeFunc(handleReshape);
    }

    void loop() {
        glutMainLoop();
    }

private:
    static void handleKeyboard(unsigned char key, int x, int y) {
        if (key == 'q' || key == 27) {
            glutDestroyWindow(glutGetWindow());
        }
    }

    static void handleReshape(int windowWidth, int windowHeight) {
        const int border = 3;
        instance->contentX = (windowWidth - instance->contentWidth) / 2;
        instance->contentY = (windowHeight - instance->contentHeight) / 2;

        glViewport(instance->contentX,
                   instance->contentY,
                   instance->contentWidth,
                   instance->contentHeight);
        glRasterPos4f(-1, -1, 0, 1);

        glViewport(instance->contentX - border,
                   instance->contentY - border,
                   instance->contentWidth + border * 2,
                   instance->contentHeight + border * 2);
    }

    static void handleDisplay(void) {
        glClearColor(0.1, 0.1, 0.1, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBegin(GL_TRIANGLE_STRIP);
        {
            glColor3f(1.f, 0.f, 0.f);
            glVertex2i(-1, -1);
            glVertex2i(1, -1);
            glVertex2i(-1, 1);
            glVertex2i(1, 1);
        }
        glEnd();

        instance->updateFunction(instance->gpuWrapper);
        const uint32_t *pixels = instance->gpuWrapper.readFramebuffer(true);
        glDrawPixels(instance->contentWidth, instance->contentHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        glutSwapBuffers();
        glutPostRedisplay();
    }

    GpuWrapper &gpuWrapper;
    UpdateFunction updateFunction;
    int contentX = 0;
    int contentY = 0;
    const int contentWidth;
    const int contentHeight;
    static inline WindowWrapper *instance = nullptr;
};

int sc_main(int argc, char *argv[]) {
    const char *vsCode = R"code(
            #vertexShader
            #input r0.xyz
            #input r1.xyz
            #output r0.xyzw
            #output r1.xyz
            #uniform r2.xyz    // rotation origin
            #uniform r3.xy     // position offset offset
            #uniform r15.xy    // rotation sin and cos

            // Prepare sine and cosine registers
            swizzle r4 r15.xxxx   // r4 = all components sin
            swizzle r5 r15.yyyy   // r5 = all components cos
            fneg    r6 r4        // r6 = all components -sin
            fneg    r7 r5        // r7 = all components -cos

            // Prepare rotation matrix
            //
            //      |  cos    0   sin   0  |
            //      |   0     1    0    0  |
            //      |  -sin   0   cos   0  |
            //      |   0     0    0    0  |
            //
            mov   r8.x  r5     // r8 = (cos, 0, sin, 0)
            mov   r8.z  r4
            finit r9.y  1.0f   // r9 = (0, 1, 0, 1)
            mov   r10.x r6     // r10 = (-sin, 0, cos)
            mov   r10.z r5

            // Multiply rotation matrix and position vector
            fsub r0.xyz r0 r2
            fdot r11.x r8  r0
            fdot r11.y r9  r0
            fdot r11.z r10 r0
            fadd r0.xyz r11 r2


            // Multiply rotation matrix and normal vector
            fdot r11.x r8  r1
            fdot r11.y r9  r1
            fdot r11.z r10 r1
            mov  r1.xyz r11

            // Add x-offset
            fadd r0.xy r0 r3

            // Set position.w = 1
            finit r0.w 1.f
        )code";
    const char *fsCode = R"code(
            #fragmentShader
            #input r0.xyzw   // fragment position
            #input r1.xyz    // normal vector
            #output r15.xyzw // color
            #uniform r2.xyz  // light position

            // Make sure normal vector is normalized
            fnorm r1 r1

            // Calculate diffuse power
            fsub  r10.xyz r2  r0       // r10 = lightDir
            fnorm r10.xyz r10
            fdot  r11.xyz r1  r10      // r11 = diffusePower
            fmax  r11.xyz r11 r15

            // Calculate diffuse light color
            finit r12.xyz 0.9 0.3 0.9  // r12 = lightColor
            fmul  r13.xyz r12 r11      // r13 = diffuseColor

            // Output
            mov  r15.xyz r13
            finit r15.w   1.0f
        )code";

    struct Vec3 {
        float x, y, z;
    };
    struct Vertex {
        Vec3 pos;
        Vec3 norm;
    };
    static_assert(sizeof(Vertex) == 6 * sizeof(float));
    Vec3 pos0{10, 30, 50};
    Vec3 pos1{50, 30, 70};
    Vec3 pos2{90, 30, 50};
    Vec3 pos3{50, 30, 30};
    Vec3 pos4{50, 60, 50};
    Vec3 norm0{-0.3841106397986879, 0.5121475197315839, -0.7682212795973759};
    Vec3 norm1{0.3841106397986879, 0.5121475197315839, -0.7682212795973759};
    Vec3 norm2{0.3841106397986879, 0.5121475197315839, 0.7682212795973759};
    Vec3 norm3{-0.3841106397986879, 0.5121475197315839, 0.7682212795973759};
    Vertex vertices[] = {
        {pos0, norm0},
        {pos4, norm0},
        {pos3, norm0},

        {pos3, norm1},
        {pos4, norm1},
        {pos2, norm1},

        {pos2, norm2},
        {pos4, norm2},
        {pos1, norm2},

        {pos1, norm3},
        {pos4, norm3},
        {pos0, norm3},
    };
    const size_t vertexCount = sizeof(vertices) / sizeof(Vertex);

    Gpu gpu{"Gpu"};
    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    GpuWrapper gpuWrapper{gpu, clock, 100, 100};

    gpuWrapper.setShaders(vsCode, fsCode);
    gpuWrapper.setVsUniform(0, Conversions::floatBytesToUint(50), Conversions::floatBytesToUint(30), Conversions::floatBytesToUint(50));
    gpuWrapper.setFsUniform(0, Conversions::floatBytesToUint(50), Conversions::floatBytesToUint(50), Conversions::floatBytesToUint(0));
    gpuWrapper.setDepth();
    gpuWrapper.setVertices((uint32_t *)vertices, vertexCount, 6);

    gpuWrapper.summarizeMemory();

    WindowWrapper::UpdateFunction updateFunction = +[](GpuWrapper &gpuWrapper) {
        static uint32_t bgColor = 0xffcccccc;
        gpuWrapper.clearDepthBuffer();
        gpuWrapper.clearFrameBuffer(&bgColor);

        static float xOffset = 0;
        static float yOffset = 0;
        gpuWrapper.setVsUniform(1, Conversions::floatBytesToUint(xOffset), Conversions::floatBytesToUint(yOffset));

        static double rotationRadians = 0;
        rotationRadians += 0.09;
        gpuWrapper.setVsUniform(2, Conversions::floatBytesToUint(sin(rotationRadians)), Conversions::floatBytesToUint(cos(rotationRadians)));

        gpuWrapper.draw();
        gpuWrapper.waitForIdle();
    };

    WindowWrapper window{gpuWrapper, updateFunction, 100, 100};
    window.loop();

    return 0;
}
