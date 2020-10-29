//
// Created by goksu on 2/25/20.
//
#include "Scene.hpp"
#include <functional>
#include <queue>

#pragma once
struct hit_payload
{
    float tNear;
    uint32_t index;
    Vector2f uv;
    Object* hit_obj;
};

struct task_data {
    int m;
    Vector3f eye_pos;
    Vector3f dir;
    int depth;
    int ssp;
    int id;
};

struct task {
    task_data data;
    std::function<void(task_data)> func;
};

class Renderer
{
public:
    void Render(const Scene& scene);
    void Run(int id);
    void CastRayTask(task_data);
    void AddTask(const task&);

private:
    std::queue<task> taskQueue;
    bool exitThread;
    std::vector<Vector3f> framebuffer;
    Scene* Renderer_scene;
    int task_id;
};
