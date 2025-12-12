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

// Shader utility functions
string loadShaderSource(const string& filepath) {
    ifstream file(filepath);
    if (!file.is_open()) {
        cerr << "Failed to open shader file: " << filepath << "\n";
        return "";
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(GLenum type, const string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        cerr << "Shader compilation failed:\n" << infoLog << "\n";
    }
    return shader;
}

GLuint createShaderProgram(const string& vertPath, const string& fragPath) {
    string vertSource = loadShaderSource(vertPath);
    string fragSource = loadShaderSource(fragPath);
    
    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSource);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSource);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        cerr << "Shader program linking failed:\n" << infoLog << "\n";
    }
    
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    
    return program;
}

struct Camera {
    float distance = 3e11f;
    float theta = M_PI / 4.0f;    // Elevation angle
    float phi = 0.0f;              // Azimuth angle
    float minDistance = 1e10f;
    float maxDistance = 6e11f;
    float sensitivity = 0.005f;
    float zoomSpeed = 0.1f;
    
    bool mousePressed = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    
    vec3 getPosition() const {
        return vec3(
            distance * sin(theta) * cos(phi),
            distance * cos(theta),
            distance * sin(theta) * sin(phi)
        );
    }
    
    mat4 getViewMatrix() const {
        return lookAt(getPosition(), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
    }
    
    mat4 getProjectionMatrix(float aspectRatio) const {
        return perspective(radians(45.0f), aspectRatio, 1e9f, 1e13f);
    }
    
    void onMouseButton(GLFWwindow* window, int button, int action) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            mousePressed = (action == GLFW_PRESS);
            if (mousePressed) {
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            }
        }
    }
    
    void onMouseMove(double xpos, double ypos) {
        if (mousePressed) {
            double dx = xpos - lastMouseX;
            double dy = ypos - lastMouseY;
            
            phi += dx * sensitivity;
            theta = glm::clamp(theta - (float)dy * sensitivity, 0.1f, (float)M_PI - 0.1f);
            
            lastMouseX = xpos;
            lastMouseY = ypos;
        }
    }
    
    void onScroll(double yoffset) {
        distance *= (1.0f - yoffset * zoomSpeed);
        distance = glm::clamp(distance, minDistance, maxDistance);
    }
};

Camera camera;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    camera.onMouseButton(window, button, action);
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    camera.onMouseMove(xpos, ypos);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.onScroll(yoffset);
}

struct Body {
    vec3 position;
    float radius;
    vec3 color;
    
    Body(vec3 pos, float r, vec3 col) : position(pos), radius(r), color(col) {}
};

vector<Body> bodies; 

struct Engine {
    GLFWwindow* window;
    int WIDTH = 800;
    int HEIGHT = 600;
    float width = 1e11;
    float height = 7.5e10;
    GLuint blackholeShader;
    GLuint gridShader;
    GLuint VAO, VBO;
    
    Engine() {
        if (!glfwInit()) {
            cerr << "GLFW init failed\n";
            exit(EXIT_FAILURE);
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Black Hole", nullptr, nullptr);
        if (!window) {
            cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);
        
        // Set up callbacks
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);
        
        glewExperimental = GL_TRUE;
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK) {
            cerr << "Failed to initialize GLEW\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        
        glViewport(0, 0, WIDTH, HEIGHT);
        
        // Create shader programs
        blackholeShader = createShaderProgram("shaders/blackhole.vert", "shaders/blackhole.frag");
        gridShader = createShaderProgram("shaders/grid.vert", "shaders/grid.frag");
        
        // Create fullscreen quad
        float vertices[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f
        };
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    
    void render(float r_s, float time) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Get camera matrices
        vec3 cameraPos = camera.getPosition();
        mat4 view = camera.getViewMatrix();
        mat4 projection = camera.getProjectionMatrix((float)WIDTH / (float)HEIGHT);
        mat4 invView = inverse(view);
        mat4 invProj = inverse(projection);
        
        // Render grid first (back layer)
        glUseProgram(gridShader);
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "invView"), 1, GL_FALSE, value_ptr(invView));
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "invProj"), 1, GL_FALSE, value_ptr(invProj));
        glUniform3fv(glGetUniformLocation(gridShader, "cameraPos"), 1, value_ptr(cameraPos));
        glUniform1f(glGetUniformLocation(gridShader, "r_s"), r_s);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        // Render black hole on top (front layer)
        glUseProgram(blackholeShader);
        glUniformMatrix4fv(glGetUniformLocation(blackholeShader, "invView"), 1, GL_FALSE, value_ptr(invView));
        glUniformMatrix4fv(glGetUniformLocation(blackholeShader, "invProj"), 1, GL_FALSE, value_ptr(invProj));
        glUniform3fv(glGetUniformLocation(blackholeShader, "cameraPos"), 1, value_ptr(cameraPos));
        glUniform1f(glGetUniformLocation(blackholeShader, "r_s"), r_s);
        glUniform1f(glGetUniformLocation(blackholeShader, "c"), c);
        glUniform1i(glGetUniformLocation(blackholeShader, "numBodies"), bodies.size());
        
        // Pass body data to shader
        for (size_t i = 0; i < bodies.size() && i < 10; ++i) {
            string posUniform = "bodies[" + to_string(i) + "].position";
            string radiusUniform = "bodies[" + to_string(i) + "].radius";
            string colorUniform = "bodies[" + to_string(i) + "].color";
            
            glUniform3fv(glGetUniformLocation(blackholeShader, posUniform.c_str()), 1, value_ptr(bodies[i].position));
            glUniform1f(glGetUniformLocation(blackholeShader, radiusUniform.c_str()), bodies[i].radius);
            glUniform3fv(glGetUniformLocation(blackholeShader, colorUniform.c_str()), 1, value_ptr(bodies[i].color));
        }
        
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }
    
    ~Engine() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(blackholeShader);
        glDeleteProgram(gridShader);
        glfwTerminate();
    }
};
Engine engine;

struct Blackhole {
    vec2 position;
    double mass;
    double r_s;

    Blackhole(vec2 pos, double m) : position(pos), mass(m), r_s(2 * G * m / (c * c)) {}
};
Blackhole SagA(vec2(0.0, 0.0), 8.54e36);



int main() {    
    float time = 0.0f;
    
    // Add some example bodies (radius ~1.27e10, same as Sag A* Schwarzschild radius)
    bodies.push_back(Body(vec3(4e11, 0.0, 0.0), 2.27e10, vec3(0.98, 0.533, 0.173)));  // Orange sphere
    bodies.push_back(Body(vec3(-4e11, 0.0, 0.0), 2.27e10, vec3(0.859, 0.208, 0.106))); // Dark orange sphere
    
    while(!glfwWindowShouldClose(engine.window)) {
        engine.render(SagA.r_s, time);
        
        glfwSwapBuffers(engine.window);
        glfwPollEvents();
        
        time += 0.016f; // Approximately 60 FPS
    }
    
    return 0;
}
