#pragma once

#include <vector>
#include <fstream>
#include <iostream>
#include "concurrentqueue.h"
#include "camera.hpp"
#include "light.hpp"
#include "object.hpp"
#include "color.hpp"
#include "utility.hpp"
#include "ray.hpp"

class Scene {
public:
  struct {
    unsigned thread_worker = std::thread::hardware_concurrency();
    unsigned trace_depth = 10;
    float trace_bias = 1e-4;
    Color environment_color = Color::GRAY;
  } config;

private:
  const Camera* camera;
  std::vector<const Light*> lights;
  std::vector<const Object*> objects;
  Color* frame;

  Color trace(const Ray &ray, float refractive_index = 1, unsigned depth = 0) const {
    if (depth > config.trace_depth) {
      return config.environment_color;
    }
    float distance = std::numeric_limits<float>::max();
    const Object* object = nullptr;
    const Light* light = nullptr;
    for (auto &o : objects) {
      float length = o->intersect(ray);
      if (length < distance) {
        distance = length;
        object = o;
      }
    }
    for (auto &l : lights) {
      float length = l->intersect(ray);
      if (length < distance) {
        distance = length;
        light = l;
      }
    }
    if (light != nullptr) {
      return light->color;
    }
    if (object == nullptr) {
      return config.environment_color;
    }
    auto color = Color::ZERO;
    auto point = ray.source + ray.direction * distance;
    auto normal = object->get_normal(point);
    for (auto &l : lights) {
      auto illuminate = l->illuminate(point + normal * config.trace_bias, objects);
      // diffusive shading
      if (object->material.k_diffusive > 0) {
        float dot = std::max(.0f, normal.dot(-illuminate.direction));
        color += object->material.k_diffusive * illuminate.intensity * dot;
      }
      // specular shading (phong's model)
      if (object->material.k_specular > 0) {
        auto reflective_direction = ray.direction.reflect(normal);
        float dot = std::max(.0f, ray.direction.dot(reflective_direction));
        color += object->material.k_specular * illuminate.intensity * powf(dot, 20.f);
      }
    }
    // reflection
    if (object->material.k_reflective > 0) {
      float k_diffuse_reflect = 0;
      if (k_diffuse_reflect > 0 && depth < 2) {
//        Vector RP = ray.direction.reflect(normal);
//        Vector RN1 = Vector(RP.z, RP.y, -RP.x);
//        Vector RN2 = RP.det(RN1);
//        Color c(0, 0, 0);
//        for (int i = 0; i < 128; ++i) {
//          float len = randf() * k_diffuse_reflect;
//          float angle = static_cast<float>(randf() * 2 * M_PI);
//          float xoff = len * cosf(angle), yoff = len * sinf(angle);
//          Vector R = (RP + RN1 * xoff + RN2 * yoff * k_diffuse_reflect).normalize();
//          Ray ray_reflect(point + R * config.trace_bias, R);
//          c += object->material.k_reflective * trace(ray_reflect, refractive_index, depth + 1);
//        }
//        color += c / 128.;
      } else {
        auto reflective_direction = ray.direction.reflect(normal);
        auto reflective_ray = Ray(point + reflective_direction * config.trace_bias, reflective_direction);
        color += object->material.k_reflective * trace(reflective_ray, refractive_index, depth + 1);
      }
    }
    // refraction
    if (object->material.k_refractive > 0) {
      auto refractive_direction = ray.direction.refract(normal, refractive_index / object->material.k_refractive_index);
      if (refractive_direction != Vector::ZERO) {
        auto refractive_ray = Ray(point + refractive_direction * config.trace_bias, refractive_direction);
        color += object->material.k_refractive * trace(refractive_ray, object->material.k_refractive_index, depth + 1);
      }
    }
    return config.environment_color + color * object->get_color(point);
  }

public:
  Scene(const Camera* camera) {
    this->camera = camera;
    this->frame = new Color[camera->height * camera->width];
  }

  void add(const Light* light) {
    lights.push_back(light);
  }

  void add(const Object* object) {
    objects.push_back(object);
  }

  void render() {
    auto start = std::chrono::high_resolution_clock::now();
    std::cerr << "start rendering with " << config.thread_worker << " thread workers";
    moodycamel::ConcurrentQueue<std::pair<int, int> > queue;
    for (int y = 0; y < camera->height; ++y)
      for (int x = 0; x < camera->width; ++x)
        queue.enqueue(std::make_pair(x, y));

    std::atomic<unsigned> counter(0);
    std::vector<std::thread> workers;
    for (int i = 0; i < config.thread_worker; ++i) {
      auto render = [&] {
        for (std::pair<unsigned, unsigned> item; queue.try_dequeue(item); ++counter) {
          auto x = item.first, y = item.second;
          auto ray = camera->ray(x, y);
          frame[y * camera->width + x] = trace(ray);
        }
      };
      workers.push_back(std::thread(render));
    }
    for (unsigned i; (i = counter.load()) < camera->width * camera->height; ) {
      auto now = std::chrono::high_resolution_clock::now();
      std::cerr << "\rrendered " << i << "/" << camera->width * camera->height
                << " pixels with " << config.thread_worker << " thread workers"
                << " in " << (now - start).count() / 1e9 << " seconds";
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    for (auto &worker : workers) {
      worker.join();
    }
    auto now = std::chrono::high_resolution_clock::now();
    std::cerr << std::endl << "done in " << (now - start).count() / 1e9 << " seconds" << std::endl;
  }

  void save(const std::string &name) const {
    std::ofstream ofs(name, std::ios::out | std::ios::binary);
    ofs << "P6\n" << camera->width << " " << camera->height << "\n255\n";
    for (unsigned i = 0; i < camera->height * camera->width; ++i) {
      ofs << static_cast<char>(clamp(frame[i].x, 0, 1) * 255)
          << static_cast<char>(clamp(frame[i].y, 0, 1) * 255)
          << static_cast<char>(clamp(frame[i].z, 0, 1) * 255);
    }
    ofs.close();
  }
};