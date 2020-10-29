//
// Created by goksu on 2/25/20.
//

#include <fstream>
#include "Scene.hpp"
#include "Renderer.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>

#define MAX_QUEUE_SIZE 1000
#define MAX_THREAD_SIZE 4

std::mutex _lock;
std::mutex _buf_lock;
std::condition_variable _no_empty;
std::condition_variable _no_full;

inline float deg2rad(const float& deg) { return deg * M_PI / 180.0; }

const float EPSILON = 0.01;

// The main render function. This where we iterate over all pixels in the image,
// generate primary rays and cast these rays into the scene. The content of the
// framebuffer is saved to a file.
void Renderer::Render(const Scene& scene)
{
    exitThread = false;
    std::vector<std::thread *> threads;
    for (int i = 0; i < MAX_THREAD_SIZE; i++) {
        std::thread *t = new std::thread([=]{
            Run(i);
        });
        threads.push_back(t);
    }

    Renderer_scene = (Scene *)(&scene);
    framebuffer.resize(scene.width*scene.height);

    float scale = tan(deg2rad(scene.fov * 0.5));
    float imageAspectRatio = scene.width / (float)scene.height;
    Vector3f eye_pos(278, 273, -800);
    int m = 0;
    task_id = 0;

    // change the spp value to change sample ammount
    int spp = 16;
    std::cout << "SPP: " << spp << "\n";
    for (uint32_t j = 0; j < scene.height; ++j) {
        for (uint32_t i = 0; i < scene.width; ++i) {
            // generate primary ray direction
            float x = (2 * (i + 0.5) / (float)scene.width - 1) *
                      imageAspectRatio * scale;
            float y = (1 - 2 * (j + 0.5) / (float)scene.height) * scale;

            Vector3f dir = normalize(Vector3f(-x, y, 1));
            for (int k = 0; k < spp; k++){
                task_data d = {m, eye_pos, dir, 0, spp};
                auto func = std::bind(&Renderer::CastRayTask, this, std::placeholders::_1);
                task t = {d, func};
                AddTask(t);
            }
            m++;
        }
        UpdateProgress(scene.height, j);
    }

    exitThread = true;
    for (auto &t: threads) {
        t->join();
    }
    UpdateProgress(scene.height, scene.height);

    // save framebuffer to file
    FILE* fp = fopen("binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i) {
        static unsigned char color[3];
        color[0] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].x), 0.6f));
        color[1] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].y), 0.6f));
        color[2] = (unsigned char)(255 * std::pow(clamp(0, 1, framebuffer[i].z), 0.6f));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);    
}

void Renderer::AddTask(const task &t) {

    while (true) {
        std::unique_lock<std::mutex> l(_lock);
        if (taskQueue.size() < MAX_THREAD_SIZE) {
            taskQueue.push(t);
            l.unlock();
            _no_empty.notify_all();
            break;
        }
        else {
            _no_full.wait(l);
        }
    }
}

void Renderer::Run(int id) {
    while(true) {
        std::unique_lock<std::mutex> l(_lock);
        if (!taskQueue.empty()) {
            task t = taskQueue.front();
            taskQueue.pop();
            l.unlock();

            t.data.id = id;
            t.func(t.data);
            _no_full.notify_all();
        }
        else if (exitThread)
            break;

        else 
            _no_empty.wait_for(l, std::chrono::milliseconds(50));
    }
}

void Renderer::CastRayTask(task_data data) {
    auto c = Renderer_scene->castRay(Ray(data.eye_pos, data.dir), data.depth) / data.ssp;

    std::unique_lock<std::mutex> l(_buf_lock);
    framebuffer[data.m] += c;
    int line = task_id / (Renderer_scene->width*data.ssp);
    task_id++;
    UpdateProgress(Renderer_scene->height, line);
}