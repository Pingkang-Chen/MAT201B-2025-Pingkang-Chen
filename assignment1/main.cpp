#include "al/app/al_App.hpp"
#include "al/graphics/al_Image.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"
using namespace al;
#include <fstream>
#include <string>

Vec3f rvec() { return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()); }
RGB rcolor() { return RGB(rnd::uniform(), rnd::uniform(), rnd::uniform()); }

// Convert HSV to cylindrical 3D coordinates for HSV cylinder mode
Vec3f hsvToCylinder(float h, float s, float v) {
    float angle = h * 2.0f * M_PI;
    float radius = s;
    float x = radius * cos(angle);
    float z = radius * sin(angle);
    float y = v;
    return Vec3f(x, y, z);
}

std::string slurp(std::string fileName);
class MyApp : public App {
    Mesh grid, rgb, hsl, mine;
    Mesh mesh; 
    ShaderProgram shader; 
    Parameter pointSize{"pointSize", 0.004, 0.0005, 0.015};

    // Vectors used for interpolation between layouts
    std::vector<Vec3f> currentPositions;
    std::vector<Vec3f> targetPositions;
    float interp = 1.0f;

    // Initialize GUI
    void onInit() override {
        auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
        auto &gui = GUIdomain->newGUI();
        gui.add(pointSize);
    }

    // Load image, generate layout meshes, and compile shaders
    void onCreate() override {
        auto image = Image("../colorful.jpg");
        if (image.width() == 0) {
            std::cout << "Image not found" << std::endl;
            exit(1);
        }

        mesh.primitive(Mesh::POINTS);
        grid.primitive(Mesh::POINTS);
        rgb.primitive(Mesh::POINTS);
        hsl.primitive(Mesh::POINTS);
        mine.primitive(Mesh::POINTS);

        // Process each pixel
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                auto pixel = image.at(x, y);
                float r = pixel.r / 255.0f;
                float g = pixel.g / 255.0f;
                float b = pixel.b / 255.0f;

                // Original 2D position
                Vec3f imagePos = Vec3f(x / float(image.width()), y / float(image.height()), 0);
                // RGB cube layout
                Vec3f rgbPos = Vec3f(r, g, b);
                // Convert to HSV and map to cylinder
                Color color(r, g, b);
                HSV hsv(color);
                Vec3f hsvPos = hsvToCylinder(hsv.h, hsv.s, hsv.v);
                // Custom layout
                Vec3f customPos = Vec3f(r + b, g - r, 1.0f - b);

                grid.vertex(imagePos);
                rgb.vertex(rgbPos);
                hsl.vertex(hsvPos);
                mine.vertex(customPos);

                grid.color(color);
                rgb.color(color);
                hsl.color(color);
                mine.color(color);

                grid.texCoord(0.1, 0);
                rgb.texCoord(0.1, 0);
                hsl.texCoord(0.1, 0);
                mine.texCoord(0.1, 0);

                // Initialize displayed mesh
                mesh.vertex(imagePos);
                mesh.color(color);
                mesh.texCoord(0.1, 0);

                // Set initial positions for interpolation
                currentPositions.push_back(imagePos);
                targetPositions.push_back(imagePos);
            }
        }

        nav().pos(0, 0, 5);

        // Compile shaders
        if (! shader.compile(slurp("../point-vertex.glsl"), slurp("../point-fragment.glsl"), slurp("../point-geometry.glsl"))) {
            printf("Shader failed to compile\n");
            exit(1);
        }
    }

    double time = 0;

    // Animate interpolation over time  
    void onAnimate(double dt) override {
        time += dt;
        if (interp < 1.0f) {
            interp += dt;
            if (interp > 1.0f) interp = 1.0f;
            for (int i = 0; i < mesh.vertices().size(); ++i) {
                // compute linear interpolation 
                mesh.vertices()[i] = currentPositions[i] * (1.0f - interp) + targetPositions[i] * interp;
            }
        }
    }

    // Draw the mesh
    void onDraw(Graphics& g) override {
        g.clear(0.1);
        g.shader(shader);
        g.shader().uniform("pointSize", pointSize);
        g.blending(true);
        g.blendTrans();
        g.depthTesting(true);
        g.draw(mesh);
    }

    // Trigger interpolation to a new layout
    void setTarget(const Mesh& target) {
        for (int i = 0; i < mesh.vertices().size(); ++i) {
            currentPositions[i] = mesh.vertices()[i];
            targetPositions[i] = target.vertices()[i];
        }
        interp = 0.0f;
    }

    bool onKeyDown(const Keyboard& k) override {
        if (k.key() == 'q') quit(); 

        if (k.key() == ' ') {
            mesh.reset();
            for (int i = 0; i < 100; ++i) {
                mesh.vertex(rvec());
                mesh.color(rcolor());
                mesh.texCoord(0.1, 0);
            }
        }

        if (k.key() == '1') setTarget(grid);     // Image layout
        if (k.key() == '2') setTarget(rgb);      // RGB cube
        if (k.key() == '3') setTarget(hsl);      // HSV cylinder
        if (k.key() == '4') setTarget(mine);     // Custom layout
        return true;
    }
};

int main() { MyApp().start(); }

std::string slurp(std::string fileName) {
    std::fstream file(fileName);
    std::string returnValue = "";
    while (file.good()) {
      std::string line;
      getline(file, line);
      returnValue += line + "\n";
    }
    return returnValue;
}
