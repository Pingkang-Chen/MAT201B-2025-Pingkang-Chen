#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"

using namespace al;

#include <fstream>
#include <vector>
using namespace std;

Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}
string slurp(string fileName); // forward declaration

struct AlloApp : App {
  Parameter pointSize{"/pointSize", "", 1.0, 0.0, 2.0};
  Parameter timeStep{"/timeStep", "", 0.1, 0.01, 0.6};
  Parameter dragFactor{"/dragFactor", "", 0.1, 0.0, 0.9};
  Parameter springK{"/springK", "", 1.0, 0.0, 10.0};
  Parameter chargeK{"/chargeK", "", 1.0, 0.0, 10.0};
  Parameter loveStrength{"/loveStrength", "", 0.5, 0.0, 5.0};

  ShaderProgram pointShader;

  //  simulation state
  Mesh mesh;  // position *is inside the mesh* mesh.vertices() are the positions
  vector<Vec3f> velocity;
  vector<Vec3f> force;
  vector<float> mass;
  vector<int> loveTarget; // index of who each particle "loves"

  void onInit() override {
    // set up GUI
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();
    gui.add(pointSize);  // add parameter to GUI
    gui.add(timeStep);
    gui.add(dragFactor);
    gui.add(springK);
    gui.add(chargeK);
    gui.add(loveStrength);
  }

  void onCreate() override {
    // compile shaders
    pointShader.compile(slurp("../point-vertex.glsl"),
                        slurp("../point-fragment.glsl"),
                        slurp("../point-geometry.glsl"));

    // c++11 "lambda" function                    
    auto randomColor = []() { return HSV(rnd::uniform(), 1.0f, 1.0f); };

    mesh.primitive(Mesh::POINTS);
    for (int _ = 0; _ < 1000; _++) {
      mesh.vertex(randomVec3f(5));
      mesh.color(randomColor());

      // float m = rnd::uniform(3.0, 0.5);
      float m = 3 + rnd::normal() / 2;
      if (m < 0.5) m = 0.5;
      mass.push_back(m);

      // using a simplified volume/size relationship
      mesh.texCoord(pow(m, 1.0f / 3), 0);

      // separate state arrays
      velocity.push_back(randomVec3f(0.1));
      force.push_back(randomVec3f(1));
      loveTarget.push_back(rnd::uniform(0, 1000));
    }

    nav().pos(0, 0, 10);
  }

  bool freeze = false;
  void onAnimate(double dt) override {
    if (freeze) return;

    vector<Vec3f> &position = mesh.vertices();

    // Spring force to surface of sphere
    for (int i = 0; i < position.size(); ++i) {
      Vec3f &pos = position[i];
      float desiredRadius = 5.0f;
      Vec3f fromCenter = pos;
      float currentRadius = fromCenter.mag();
      float displacement = currentRadius - desiredRadius;
      Vec3f springDirection = fromCenter.normalized();
      force[i] += -springK * displacement * springDirection;
    }

    // Coulomb repulsion between all pairs
    for (int i = 0; i < position.size(); ++i) {
      for (int j = i + 1; j < position.size(); ++j) {
        Vec3f diff = position[i] - position[j];
        float distSqr = diff.magSqr() + 0.01; // avoid div by 0
        Vec3f dir = diff.normalized();
        Vec3f repulsion = chargeK / distSqr * dir;
        force[i] += repulsion;
        force[j] -= repulsion;
      }
    }

    // Drag force
    for (int i = 0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragFactor;
    }

    // Asymmetrical "love" force
    for (int i = 0; i < position.size(); i++) {
      int targetIndex = loveTarget[i];
      if (targetIndex >= 0 && targetIndex < position.size()) {
        Vec3f dir = position[targetIndex] - position[i];
        force[i] += loveStrength.get() * dir.normalized();;
      }
    }

    // Integration (semi-implicit Euler)
    for (int i = 0; i < velocity.size(); i++) {
      velocity[i] += force[i] / mass[i] * timeStep;
      position[i] += velocity[i] * timeStep;
    }

    // clear all accelerations (IMPORTANT!!)
    for (auto &f : force) f.set(0);
  }

  bool onKeyDown(const Keyboard &k) override {
    if (k.key() == ' ') freeze = !freeze;
    if (k.key() == '1') {
      // introduce some "random" forces
      for (int i = 0; i < velocity.size(); i++) {
        force[i] += randomVec3f(1);
      }
    }
    return true;
  }

  void onDraw(Graphics &g) override {
    g.clear(0.3);
    g.shader(pointShader);
    g.shader().uniform("pointSize", pointSize / 100);
    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);
    g.draw(mesh);
  }
};

int main() {
  AlloApp app;
  app.configureAudio(48000, 512, 2, 0);
  app.start();
}

string slurp(string fileName) {
  fstream file(fileName);
  string returnValue = "";
  while (file.good()) {
    string line;
    getline(file, line);
    returnValue += line + "\n";
  }
  return returnValue;
}
