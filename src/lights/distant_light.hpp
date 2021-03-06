#pragma once

#include "light.hpp"

class DistantLight : public Light {
private:
  Vector direction;

public:
  DistantLight(const Color &color, float intensity, const Vector &direction) : Light(color, intensity) {
    this->direction = direction.normalize();
  }

  float intersect(const Ray &ray) const {
    return std::numeric_limits<float>::max();
  };

  Illumination illuminate(const Vector &position, const std::vector<const Object*> &objects) const {
    auto ray = Ray(position, -direction);
    if (ray.intersect(objects) < std::numeric_limits<float>::max()) {
      return {.direction = direction, .intensity = color * intensity};
    } else {
      return {.direction = Vector::ZERO, .intensity = Color::ZERO};
    }
  }
};