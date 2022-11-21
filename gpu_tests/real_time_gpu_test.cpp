#include "gpu/gpu.h"
#include "gpu/isa/assembler/assembler.h"
#include "gpu/util/vcd_trace.h"
#include "gpu/util/conversions.h"

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
            #uniform r2.xy

            // Set position.w = 1
            finit r0.w 1.f

            // y-flip
            fsub r0.y r2 r0
        )code";
    const char *fsCode = R"code(
            #fragmentShader
            #input r0.xyzw
            #input r1.xyz
            #output r1.xyzw

            // Set color.alpha = 1
            finit r12.w 1.f
        )code";

    struct Vertex {
        float x, y, z, r, g, b;
    };
    Vertex vertices[] = {
        Vertex{10, 10, 40, 1.0, 0.0, 0.0},
        Vertex{45, 80, 90, 0.0, 0.0, 1.0},
        Vertex{90, 10, 40, 0.0, 1.0, 0.0},

        Vertex{10, 60, 50, 0.5, 0.5, 0.5},
        Vertex{90, 40, 50, 1.0, 1.0, 1.0},
        Vertex{45, 20, 50, 0.0, 0.0, 0.0},
    };

    Gpu gpu{"Gpu"};
    sc_clock clock("clock", 1, SC_NS, 0.5, 0, SC_NS, true);
    GpuWrapper gpuWrapper{gpu, clock, 100, 100};

    gpuWrapper.setShaders(vsCode, fsCode);
    gpuWrapper.setVsUniform(0, Conversions::floatBytesToUint(100), Conversions::floatBytesToUint(100));
    gpuWrapper.setDepth();
    gpuWrapper.setVertices((uint32_t *)vertices, 6, 6);

    gpuWrapper.summarizeMemory();

    WindowWrapper::UpdateFunction updateFunction = +[](GpuWrapper &gpuWrapper) {
        static int frameCount = 0;
        printf("Frame %d\n", frameCount++);

        static uint32_t bgColor = 0xffcccccc;
        gpuWrapper.clearDepthBuffer();
        gpuWrapper.clearFrameBuffer(&bgColor);
        gpuWrapper.draw();
    };

    WindowWrapper window{gpuWrapper, updateFunction, 100, 100};
    window.loop();

    return 0;
}
