#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <fstream>
#include <sstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using namespace glm;
using namespace std;

double c = 299792458.0;
double G = 6.67430e-11;

struct Engine {
    GLFWwindow* window;
    int WIDTH = 800;
    int HEIGHT = 600;
    float width = 1e11;
    float height = 7.5e10;
    
    Engine() {
        if (!glfwInit()) {
            cerr << "GLFW init failed\n";
            exit(EXIT_FAILURE);
        }

        window = glfwCreateWindow(WIDTH, HEIGHT, "Black Hole", nullptr, nullptr);
        if (!window) {
            cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);
        glViewport(0, 0, WIDTH, HEIGHT);
        
    }

    void run() {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double left = -width;
        double right = width;
        double bottom = -height;
        double top = height;
        glOrtho(left, right, bottom, top, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
};
Engine engine;

struct Blackhole {
    vec2 position;
    double mass;
    double r_s;


    Blackhole(vec2 pos, double m) : position(pos), mass(m), r_s(2 * G * m / (c * c)) {}

    void draw() {
        glColor3f(1.0f, 0.0f, 0.0f);

        glBegin(GL_TRIANGLE_FAN);

        glVertex2f(position.x, position.y); 
        for (int i = 0; i <= 100; ++i) {
            double angle = i * 2.0f * M_PI / 100;
            double x = position.x + r_s * cos(angle);
            double y = position.y + r_s * sin(angle);
            glVertex2f(x, y);
        }

        glEnd();
    }
};
Blackhole SagA(vec2(0.0, 0.0), 8.54e36); 

struct Ray {
    // -- cartesian coords -- //
    double x;   double y;
    // -- polar coords -- //
    double r;   double phi;
    double dr;  double dphi; // velocities
    double d2r; double d2phi;

    vec2 dir;
    vector<vec2> trail;

    Ray(vec2 pos, vec2 direction) : x(pos.x), y(pos.y), dir(direction) {
        r = hypot(x, y);
        phi = atan(y, x);
        dr = c * cos(phi) + dir.y * sin(phi);
        dphi = ( -c * sin(phi) + dir.x * cos(phi) ) / r;
        d2r = 0.0;
        d2phi = 0.0;
        trail.push_back(vec2(x, y));
    }

    void draw() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.0f);

        size_t N = trail.size();
        if (N < 2) return;

        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < N; ++i) {
            float alpha = float(i) / float(N - 1);
            glColor4f(1.0f, 1.0f, 1.0f, std::max(alpha, 0.05f));
            glVertex2f(trail[i].x, trail[i].y);
        }
        glEnd();
    }

    void step(double r_s, double dλ) {
        if (r < r_s) return;

        dr   += d2r * dλ;
        dphi += d2phi * dλ;
        r    += dr * dλ;
        phi  += dphi * dλ;


        x = r * cos(phi);
        y = r * sin(phi);

        trail.push_back(vec2(x, y));
    }
};
vector<Ray> rays;

void geodesic(Ray& ray, double rhs[4], double r_s) {
    double r = ray.r;
    double phi = ray.phi;
    double dr = ray.dr;
    double dphi = ray.dphi;

    ray.d2r = r * dphi * dphi - (c*c * r_s) / (2 * r * r);
    ray.d2phi = -2.0 * dr * dphi / r;
}

void addState(const double a[6], const double b[6], double factor, double out[6]) {
    for (int i = 0; i < 6; i++)
        out[i] = a[i] + b[i] * factor;
}

void rk4Step(Ray& ray, double dλ, double rs) {
    double y0[6] = { ray.r, ray.phi, ray.dr, ray.dphi };
    double k1[6], k2[6], k3[6], k4[6], temp[6];

    geodesic(ray, k1, rs);
    addState(y0, k1, dλ/2.0, temp);
    Ray r2 = ray; r2.r = temp[0]; r2.phi = temp[1];  r2.dr = temp[2]; r2.dphi = temp[3];
    geodesic(r2, k2, rs);

    addState(y0, k2, dλ/2.0, temp);
    Ray r3 = ray; r3.r = temp[0]; r3.phi = temp[1]; r3.dr = temp[2]; r3.dphi = temp[3];
    geodesic(r3, k3, rs);

    addState(y0, k3, dλ, temp);
    Ray r4 = ray; r4.r = temp[0]; r4.phi = temp[1]; r4.dr = temp[2]; r4.dphi = temp[3];
    geodesic(r4, k4, rs);

    ray.r      += (dλ/6.0)*(k1[0] + 2*k2[0] + 2*k3[0] + k4[0]);
    ray.phi    += (dλ/6.0)*(k1[2] + 2*k2[2] + 2*k3[2] + k4[2]);
    ray.dr     += (dλ/6.0)*(k1[3] + 2*k2[3] + 2*k3[3] + k4[3]);
    ray.dphi   += (dλ/6.0)*(k1[5] + 2*k2[5] + 2*k3[5] + k4[5]);
}


int main() {    
    for (float y = -engine.height; y < engine.height; y += 1e10) {

        rays.push_back(Ray(vec2(-engine.width, y), vec2(1.0f, 0.0f)));
    }

    while(!glfwWindowShouldClose(engine.window)) {
        engine.run();
        SagA.draw();

        for (auto& ray : rays) {
            rk4Step(ray, 1e-1, SagA.r_s);
            ray.draw();
            ray.step(SagA.r_s, 1e-1);
        }

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }
    return 0;
}
