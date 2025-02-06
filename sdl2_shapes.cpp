#include <SDL2/SDL.h>

#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace sdl2 {

class Initializer {
 public:
  explicit Initializer(Uint32 flags) {
    if (SDL_Init(flags) != 0) {
      throw std::runtime_error(SDL_GetError());
    }
  }
  ~Initializer() { SDL_Quit(); }
};

template <auto CDeleter>
struct Deleter {
  template <class T>
  void operator()(T* o) const noexcept {
    CDeleter(o);
  }
};

struct Window {
  explicit Window(const char* title, int width, int height)
      : width_(width),
        height_(height),
        impl_{SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, width_, height_,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)} {
    if (!impl_) {
      throw std::runtime_error(SDL_GetError());
    }
  }

  auto handle() { return impl_.get(); }

 private:
  const int width_;
  const int height_;
  const std::unique_ptr<SDL_Window, sdl2::Deleter<&SDL_DestroyWindow>> impl_;
};

struct Color {
  SDL_Color color;
};

struct Renderer {
  explicit Renderer(Window& window)
      : impl_(SDL_CreateRenderer(
            window.handle(), -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)) {
    if (!impl_) throw std::runtime_error(SDL_GetError());
  }

  auto handle() { return impl_.get(); }

  void present() { SDL_RenderPresent(impl_.get()); }

  void setColor(Color c) {
    SDL_SetRenderDrawColor(impl_.get(), c.color.r, c.color.g, c.color.b,
                           c.color.a);
  }

  Color getColor() const {
    Color c;
    SDL_GetRenderDrawColor(impl_.get(), &c.color.r, &c.color.g, &c.color.b,
                           &c.color.a);
    return c;
  }

  void clearCanvas() {
    SDL_SetRenderDrawColor(impl_.get(), 255, 255, 255, 255);
    SDL_RenderClear(impl_.get());
  }

 private:
  const std::unique_ptr<SDL_Renderer, sdl2::Deleter<&SDL_DestroyRenderer>>
      impl_;
};

struct Sphere {
  SDL_Point center;
  int radius;
};

void render(Renderer& r, const Sphere& s) {
  auto renderer = r.handle();
  int centerX = s.center.x;
  int centerY = s.center.y;
  // Bresenham's midpoint circle algorithm
  int radius = s.radius;
  for (int y = -radius; y <= radius; ++y) {
    int xLimit = static_cast<int>(sqrt(radius * radius - y * y));
    SDL_RenderDrawLine(renderer, centerX - xLimit, centerY + y,
                       centerX + xLimit, centerY + y);
  }
}

struct Triangle {
  SDL_Vertex vertices[3];
};

void render(Renderer& r, const Triangle& t) {
  // TODO: find out how to render triangle without copying it
  auto in = t;
  static const auto do_copy = [](SDL_Color& to, Color from) {
    memcpy(&to, &from.color, sizeof(Color));
  };
  do_copy(in.vertices[0].color, r.getColor());
  do_copy(in.vertices[1].color, r.getColor());
  do_copy(in.vertices[2].color, r.getColor());
  SDL_RenderGeometry(r.handle(), nullptr, in.vertices, 3, nullptr, 0);
}

struct Rectangle {
  SDL_Rect rectangle;
};

void render(Renderer& r, const Rectangle& rect) {
  SDL_RenderFillRect(r.handle(), &rect.rectangle);
}

struct ShapeRenderer {
  template <class T>
  ShapeRenderer(T shape)
      : object_(std::make_shared<ConcreteShape<T>>(std::move(shape))) {}

  void render(Renderer& r) const { object_->render(r); };

 private:
  struct AbstractShape {
    virtual void render(Renderer& r) const = 0;
  };
  template <class T>
  struct ConcreteShape final : AbstractShape {
    explicit ConcreteShape(T s) : shape(std::move(s)) {}
    T shape;
    void render(Renderer& r) const override {
      using sdl2::render;
      render(r, shape);
    }
  };
  std::shared_ptr<const AbstractShape> object_;
};

}  // namespace sdl2

namespace util {

template <class T>
T random(T from, T to) {
  static std::random_device r;
  static std::default_random_engine e1(r());
  std::uniform_real_distribution<float> uniform_dist(from, to);
  return static_cast<T>(uniform_dist(e1));
}

template <class Shape>
std::enable_if_t<!std::is_arithmetic_v<Shape>, Shape> random(int windowWidth,
                                                             int windowHeight);

template <>
sdl2::Rectangle random<sdl2::Rectangle>(int windowWidth, int windowHeight) {
  int w = 150;
  int h = 100;
  int maxX = windowWidth - 150;
  int maxY = windowHeight - 100;
  int x = random(0, maxX > 0 ? maxX : 0);
  int y = random(0, maxY > 0 ? maxY : 0);
  return {x, y, w, h};
}

template <>
sdl2::Triangle random<sdl2::Triangle>(int windowWidth, int windowHeight) {
  SDL_Vertex v1, v2, v3;
  v1.position = {random(0.f, 0.f + windowWidth),
                 random(0.f, 0.f + windowHeight)};
  v2.position = {random(0.f, 0.f + windowWidth),
                 random(0.f, 0.f + windowHeight)};
  v3.position = {random(0.f, 0.f + windowWidth),
                 random(0.f, 0.f + windowHeight)};
  return {v1, v2, v3};
}

template <>
sdl2::Sphere random<sdl2::Sphere>(int windowWidth, int windowHeight) {
  int margin = random(0, windowHeight / 2);
  int x = random(margin, windowWidth - margin);
  int y = random(margin, windowHeight - margin);
  return {{x, y}, margin};
}

template <class T>
T random();

template <>
sdl2::Color random() {
  return {util::random<Uint8>(0, 255), util::random<Uint8>(0, 255),
          util::random<Uint8>(0, 255), util::random<Uint8>(0, 255)};
}

};  // namespace util

int main() try {
  using namespace sdl2;

  Initializer sdl(SDL_INIT_VIDEO);
  const int windowWidth = 640u;
  const int windowHeight = 480u;
  Window window("Shapes!", windowWidth, windowHeight);
  Renderer renderer{window};

  std::vector<std::pair<ShapeRenderer, Color>> shapes;
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
        break;
      }
      if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
          case SDLK_s:
            shapes.emplace_back(
                util::random<sdl2::Sphere>(windowWidth, windowHeight),
                util::random<Color>());
            break;
          case SDLK_r:
            shapes.emplace_back(
                util::random<sdl2::Rectangle>(windowWidth, windowHeight),
                util::random<Color>());
            break;
          case SDLK_t:
            shapes.emplace_back(
                util::random<sdl2::Triangle>(windowWidth, windowHeight),
                util::random<Color>());
            break;
        }
      }
    }

    renderer.clearCanvas();

    for (const auto& [shape, color] : shapes) {
      renderer.setColor(color);
      shape.render(renderer);
    }

    renderer.present();
  }
  return EXIT_SUCCESS;
} catch (const std::exception& ex) {
  std::cerr << "Error: " << ex.what() << std::endl;
  return EXIT_FAILURE;
}
