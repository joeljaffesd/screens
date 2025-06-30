// Joel A. Jaffe 2025-06-29
// Particle Life implementation for a screen saver
//Based on https://www.youtube.com/watch?v=xiUpAeos168&list=PLZ1w5M-dmhlGWtqzaC2aSLfQFtp0Dz-F_&index=3

#include "al/app/al_App.hpp"
#include "al/math/al_Random.hpp"
#include <fstream>
#include "shaders.h"

float fMap(float value, float in_min, float in_max, float out_min, float out_max) { // custom mapping function
  return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float fWrap(float input, float scope) { // custom wrapping function
    float lowerBound = -1 * scope;
    float range = 2 * scope;
    return lowerBound + fmodf((input - lowerBound + range), range);
}

al::Vec2f randomVec2f(float scale) { // <- Function that returns a Vec2f containing random coords
  return al::Vec2f(al::rnd::uniformS(), al::rnd::uniformS()) * scale;
} 

struct Particle { // Particle struct
  int type;
  al::Vec2f position; 
  al::Vec2f velocity;
};

struct MyApp : public al::App {
  al::Parameter simScale{"/simScale", "", 1.f, 0.00001f, 1.f}; // <- creates GUI parameter
  al::Mesh verts; // create mesh for visualzing particles
  al::ShaderProgram pointShader;

  static const int numTypes = 6; // numTypes
  int numParticles = 1000; // numParticles (1000 seems to be the limit for my M2 Max)
  float colorStep = 1.f / numTypes; // colorStrep
  float K = 0.05; // make smaller to slow sim (0.05 looks good for simScale around 1)
  float friction = 0.6; // make smaller to slow sim (0.6 looks good for a simScale around 1)
  float forces[numTypes][numTypes]; // forces table
  float minDistances[numTypes][numTypes]; // minDistances table
  float radii[numTypes][numTypes]; // radii table

  std::vector<Particle> swarm; // swarm vector

  void setParameters(int numTypes) { // define setParams function (seems to be working)
    for (int i = 0; i < numTypes; i++) {
      for (int j = 0; j < numTypes; j++) {
        forces[i][j] = al::rnd::uniform<float>(.01, .003); // .01, .003 for simScale of 1
        if (al::rnd::uniformi(1, 100) < 50) {
          forces[i][j] *= -1;
        }
        minDistances[i][j] = al::rnd::uniform<float>(.1, .05); // .1, .05 for simScale of 1
        radii[i][j] = al::rnd::uniform<float>(.5, .15); // .5, .15 for simScale of 1
        // std::cout << "forces[" << i << "][" << j << "]: " << forces[i][j] << std::endl;
        // std::cout << "minDistances[" << i << "][" << j << "]: " << minDistances[i][j] << std::endl;
        // std::cout << "radii[" << i << "][" << j << "]: " << radii[i][j] << std::endl;
      }
    }
  }

  void onCreate() {
    fullScreen(true); // <- set full screen mode
    cursorHide(true); // <- hide cursor
    verts.primitive(al::Mesh::POINTS); // skin mesh as points
    for (int i = 0; i < numParticles; i++) {  // for each iter...
      Particle particle; // initialzie a particle
      particle.type = al::rnd::uniformi(0, numTypes - 1); // give random type
      particle.position = randomVec2f(simScale); // give random pos within simScale 
      particle.velocity = 0; // give initial velocity of 0
      swarm.push_back(particle); // append to swarm vector

      verts.vertex(particle.position); // append particle to mesh 
      verts.texCoord(pow(1.f, 1.0f / 3), 0);  // s, t
      verts.color(al::HSV(particle.type * colorStep, 1.f, 1.f)); // color based on type
    }
    setParameters(numTypes); // initial params
    pointShader.compile(vert, frag, geo); // compile shaders
  }


  bool freeze = false; // <- for pausing sim
  double phase = 0;
  void onAnimate(double dt) {
    if (freeze) return; // <- if freeze is true, then pause sim
    phase += dt;
    if (phase > 7) {
      setParameters(numTypes);
      phase = 0;
    }

    for (int i = 0; i < numParticles; i++) { // for each particle, ~60fps...

      float dis = 0; // initialize dis
      al::Vec2f direction = 0; // initialize direction
      al::Vec2f totalForce = 0; // initialize totalForce
      al::Vec2f acceleration = 0; // initialize acceleration
      
      for (int j = 0; j < numParticles; j++) {
        if (i == j) {continue;} // don't have particles calculate forces on themselves
        direction *= 0; // set direction to 0
        direction += swarm[j].position; // direction == j particle 
        direction -= swarm[i].position; // direction -= current particle
        
        // make sure direction is measured with toroidal wrapping
        if (direction[0] > simScale) { 
          direction[0] -= 2 * simScale;
        }
        if (direction[0] < -1 * simScale) {
          direction[0] += 2 * simScale;
        }
        if (direction[1] > simScale) {
          direction[1] -= 2 * simScale;
        }
        if (direction[1] < -1 * simScale) {
          direction[1] += 2 * simScale;
        }
        
        dis = direction.mag(); // Euclidian distance between particles
        direction.normalize(); // normalize to unit vector

        if (dis < minDistances[swarm[i].type][swarm[j].type]) { // separation forces (personal space)
          al::Vec2f force = direction; 
          force *= abs(forces[swarm[i].type][swarm[j].type]) * -3; // calculate repulsion force based on type
          force *= fMap(dis, 0, minDistances[swarm[i].type][swarm[j].type], 1, 0); // map based on distance
          force *= K; // scale by K
          totalForce += force; // add  to totalForce
        } 
        
        if (dis < radii[swarm[i].type][swarm[j].type]) { // love/hate forces
          al::Vec2f force = direction; 
          force *= forces[swarm[i].type][swarm[j].type]; // calculate force based on type
          force *= fMap(dis, 0, radii[swarm[i].type][swarm[j].type], 1, 0); // map based on distance (flip last two arguments?)
          force *= K; // scale by K
          totalForce += force; // add to totalForce
        } 
        
      } 

      acceleration += totalForce; // integrate totalForce
      swarm[i].velocity += acceleration; // integrate acceleration
      swarm[i].velocity *= friction; // apply friction here?
      swarm[i].position += swarm[i].velocity; // integrate velocity
      
      swarm[i].position[0] = fWrap(swarm[i].position[0], simScale); // wrapping x-dim
      swarm[i].position[1] = fWrap(swarm[i].position[1], simScale); // wrapping y-dim
      
      //swarm[i].velocity *= friction; // or apply friction here?
      verts.vertices()[i] = swarm[i].position; // skin mesh
      
    } 

  } 
  
  bool onKeyDown(const al::Keyboard &k) override {
    quit();
    return true; // <- quit on key down
  }

  bool onMouseDown(const al::Mouse &m) override {
    quit();
    return true; // <- quit on mouse down
  }

  void onDraw(al::Graphics &g) { 
    g.clear(0); // black background
    g.camera(al::Viewpoint::IDENTITY); // Perspective camera
    g.shader(pointShader);
    g.shader().uniform("pointSize", 0.005f);
    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);
    g.draw(verts); // draw verts
    g.camera(al::Viewpoint::ORTHO_FOR_2D); // Ortho [0:width] x [0:height]
  }
};

int main() {
  MyApp app;
  app.title("Particle Life");
  app.configureAudio(0, 0, 0, 0); // disable audio
  app.start();
}