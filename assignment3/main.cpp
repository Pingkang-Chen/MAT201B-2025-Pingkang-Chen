#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"
#include "al/math/al_Vec.hpp"

using namespace al;

#include <vector>
#include <cmath>

struct Boid {
  Nav nav;
  float size;
  int interest = -1;
  Color color;
  
  // Adding separate force vectors to better demonstrate the flocking rules
  void update(const std::vector<Boid>& neighbors, const Vec3f& target, float dt,
              float cohesionWeight, float alignmentWeight, float separationWeight, 
              float neighborRadius) {
    Vec3f cohesionForce(0), alignmentForce(0), separationForce(0);
    int count = 0;
    
    // Find neighbors within radius
    for (auto& n : neighbors) {
      if (&n == this) continue;
      float d = (nav.pos() - n.nav.pos()).mag();
      if (d < neighborRadius) {
        cohesionForce += n.nav.pos();
        alignmentForce += n.nav.uf();
        separationForce += (nav.pos() - n.nav.pos()) / (d * d + 0.01);
        ++count;
      }
    }
    
    // Apply flocking rules if neighbors exist
    if (count > 0) {
      cohesionForce /= count;  // Center of mass
      alignmentForce /= count; // Average heading
      
      // Apply user-controlled weights to each force
      nav.faceToward(cohesionForce, cohesionWeight);           // COHESION
      nav.faceToward(nav.pos() + alignmentForce, alignmentWeight); // ALIGNMENT  
      nav.nudgeToward(nav.pos() + separationForce, separationWeight); // SEPARATION
    } else {
      // If no neighbors, move toward target
      nav.faceToward(target, 0.03);
    }
    
    nav.moveF(1.0);
    nav.step(dt);
  }
};

struct AlloApp : App {
  // USER INTERFACE CONTROLS FOR FLOCKING PARAMETERS
  Parameter timeStep{"/timeStep", "", 0.1, 0.01, 0.6};
  Parameter neighborRadius{"/neighborRadius", "", 3.0, 1.0, 10.0};
  Parameter cohesionWeight{"/cohesionWeight", "", 0.05, 0.0, 0.2};
  Parameter alignmentWeight{"/alignmentWeight", "", 0.05, 0.0, 0.2};
  Parameter separationWeight{"/separationWeight", "", 0.02, 0.0, 0.2};
  Parameter aestheticFactor{"/aestheticFactor", "", 0.5, 0.0, 1.0};

  Mesh boidMesh;
  Light light;
  Material material;
  std::vector<Boid> boids;
  Vec3f target;
  double time = 0;
  bool paused = false;
  float hue = 0.0f;

  void onInit() override {
    auto guiDomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto& gui = guiDomain->newGUI();
    // Add ALL parameters to UI
    gui.add(timeStep);
    gui.add(neighborRadius);
    gui.add(cohesionWeight);
    gui.add(alignmentWeight);
    gui.add(separationWeight);
    gui.add(aestheticFactor);
  }

  void onCreate() override {
    nav().pos(0, 0, 20);
    
    // Create asymmetrical shape for orientation mimicry
    addCone(boidMesh);
    // Make it more asymmetrical by scaling differently in different axes
    boidMesh.scale(0.3, 0.3, 0.5); 
    boidMesh.generateNormals();
    
    light.pos(0, 10, 10);

    for (int i = 0; i < 100; ++i) {
      Boid b;
      b.nav.pos() = rnd::ball<Vec3f>() * 5.0f;
      b.nav.quat().set(rnd::uniformS(), rnd::uniformS(), rnd::uniformS(), rnd::uniformS()).normalize();
      b.size = rnd::uniform(0.05f, 1.0f);
      b.color = HSV(rnd::uniform(), 0.6, 0.9);
      boids.push_back(b);
    }
    target = Vec3f(0, 0, 0);
  }

  void wrapPosition(Vec3d& pos, double limit = 20.0) {
    for (int d = 0; d < 3; ++d) {
      if (pos[d] > limit) pos[d] = -limit;
      if (pos[d] < -limit) pos[d] = limit;
    }
  }

  void onAnimate(double dt) override {
    if (paused) return;
    time += dt;
    if (time > 5.0) {
      target = rnd::ball<Vec3f>() * 10.0f;
      time = 0;
    }

    // Aesthetic element - color shifting based on time
    hue += 0.01f;
    if (hue > 1.0f) hue -= 1.0f;

    for (size_t i = 0; i < boids.size(); ++i) {
      if (i == 0) {
        // RULE-BREAKING BOID: circles around origin instead of flocking
        Vec3f dir = boids[i].nav.pos().cross(Vec3f(0, 1, 0));
        dir.normalize();
        boids[i].nav.faceToward(boids[i].nav.pos() + dir, 0.1);
        boids[i].nav.moveF(0.5);
        boids[i].nav.step(dt);
        
        // Make the rule breaker visually distinct
        boids[i].color = RGB(1, 0, 0); // Always red
      } else {
        // Regular flocking boids
        boids[i].update(
          boids, 
          target, 
          timeStep, 
          cohesionWeight, 
          alignmentWeight, 
          separationWeight, 
          neighborRadius
        );
      }

      // Wrap boid position inside bounding box
      wrapPosition(boids[i].nav.pos());

      // Aesthetic effect: shifting colors based on position and time
      if (i > 0) { // Skip the rule-breaker
        // Use the aestheticFactor parameter to control the effect
        float colorShift = sin(boids[i].nav.pos().mag() * 0.2 + time);
        boids[i].color = HSV(
          fmod(hue + i * 0.01f, 1.0f), 
          0.6, 
          0.9 * (1.0f - aestheticFactor) + aestheticFactor * (0.5f + 0.5f * colorShift)
        );
      }
    }
  }

  void onDraw(Graphics& g) override {
    g.clear(0.2);
    g.depthTesting(true);
    g.lighting(true);
    g.light(light);
    g.material(material);

    for (auto& b : boids) {
      g.pushMatrix();
      g.translate(b.nav.pos());
      g.rotate(b.nav.quat());
      g.scale(b.size);
      g.color(b.color);
      g.draw(boidMesh);
      g.popMatrix();
    }
  }

  bool onKeyDown(const Keyboard& k) override {
    if (k.key() == ' ') paused = !paused;
    return true;
  }
};

int main() {
  AlloApp app;
  app.configureAudio(48000, 512, 2, 0);
  app.start();
}