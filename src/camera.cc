#include "hw0.hh"
#include "objs.hh"
#include "lights.hh"
#include <vector>
#include <limits>
#include "camera.hh"
#include <chrono>
#include <random>

using namespace std;

Ray
Camera::ray(double i, double j)
{
  double su = this->l + (this->r - this->l) * (i + 0.5) / this->pw;
  double sv = this->b + (this->t - this->b) * (j + 0.5) / this->ph;

  float3 dir = this->dir * this->d + this->u * su + this->v * sv;
  dir.normalize_();

  return Ray(dir.x, dir.y, dir.z,
             this->pos.x, this->pos.y, this->pos.z);
}

inline void
Camera::accum_pixel(int i, int j, const float3& rgba)
{
  int nj = ph - j - 1;
  pixels[nj][i].r += rgba.x;
  pixels[nj][i].g += rgba.y;
  pixels[nj][i].b += rgba.z;
}

inline void
Camera::set_pixel(int i, int j, const float3& rgb)
{
  int nj = ph - j - 1;
  pixels[nj][i].r = rgb.x;
  pixels[nj][i].g = rgb.y;
  pixels[nj][i].b = rgb.z;
  pixels[nj][i].a = 1.0;
}

void
Camera::copy(const Camera& other)
{
#define copy_prop(what) this->what = other.what
  copy_prop(pos);
  copy_prop(dir);
  copy_prop(u);
  copy_prop(v);
  copy_prop(w);
  copy_prop(d);
  copy_prop(iw);
  copy_prop(ih);
  copy_prop(pw);
  copy_prop(ph);
  copy_prop(r);
  copy_prop(l);
  copy_prop(t);
  copy_prop(b);

  pixels.resizeErase(ph, pw);
}

Camera::Camera(const Camera& other)
{
  copy(other);
}

void
Camera::specular_reflection(const Intersection& in, const Ray& r,
                            float3& clr, int recursion_depth) const
{
  if (recursion_depth > MAXRECUR) {
    return;
  }

  float3 rf = r.d - in.n * 2 * r.d.dot(in.n);
  float3 pp = in.p + rf * CAMEPSILON;

  float3 acc(0., 0., 0.);
  ray_color(Ray(rf, pp), acc, recursion_depth + 1);
  clr += in.sp->mat->s.pll_mul(acc);
}

bool
Camera::is_shadowed(const Light& lgh, const Intersection& in,
                    const Ray& pr) const
{
  float3 l = lgh.l(in);

  float3 pp = in.p + pr.d * CAMEPSILON;
  Ray r(l.x, l.y, l.z,
        pp.x, pp.y, pp.z);

  vector<Intersection> ins;
  for (int i = 0; i < scene->shapes.size(); ++i) {
    if (scene->shapes[i] == in.sp) {
      continue;
    }

    if (scene->shapes[i]->test_with(r, ins) > 0) {
      return true;
    }
  }

  return false;
}

void
Camera::ray_color(const Ray& r, float3& clr, int recursion_depth) const
{
  vector<Intersection> ins;
  const int count = r.test_with(scene->shapes, ins);

  if (count <= 0) {
    return;
  }

  int idx = -1;
  double min_dist = numeric_limits<double>::max();

  for (int k = 0; k < ins.size(); ++k) {
    const double dist = pos.sq_dist(ins[k].p);

    if (min_dist > dist) {
      idx = k;
      min_dist = dist;
    }
  }

  if (idx < 0) {
    return;
  }

  const Intersection& in = ins[idx];
  const Material& mat = *(in.sp->mat);
  for (int k = 0; k < scene->lights.size(); ++k) {
    const Light& lgh = *(scene->lights[k]);

    if (lgh.type() == LightType::point
        || lgh.type() == LightType::directional) {
      if (is_shadowed(lgh, in, r)) {
        // printf("shadowed: %s\n", Shape::to_s(in.sp->type()).c_str());
      } else {
        // printf("not shadowed: %s\n", Shape::to_s(in.sp->type()).c_str());

        clr += mat.diffuse(lgh, in);

        const float3& nd = r.d * -1;
        clr += mat.specular(lgh, in, nd);

#ifdef SPECULAR_REFLECTION
        if (FEQ(mat.s.x, 0.0) && FEQ(mat.s.y, 0.0) && FEQ(mat.s.z, 0.0)) {
        } else {
          specular_reflection(in, r, clr, recursion_depth);
        }
#endif
      }
    } else if (lgh.type() == LightType::ambient) {
      clr += mat.ambient(lgh);
    }
  }
}

void
Camera::render()
{
  printf("Shyoujyou rendering...\n");

  auto begin = chrono::steady_clock::now();

  double total = pw * ph;

  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0.0, 0.999999999);

  long cnt = 0;
  for (int x = 0; x < pw; ++x) {
    for (int y = 0; y < ph; ++y) {
      printf("\rRendered %0.03f%%", ((++cnt) / total) * 100.);

      float3 clr(0., 0., 0.);
#ifdef ANTIALIASING
      for (int p = 0; p < NSAMPLE; ++p) {
        for (int q = 0; q < NSAMPLE; ++q) {
          Ray r = ray(x + (p + dist(e2)) / NSAMPLE,
                      y + (q + dist(e2)) / NSAMPLE);

          ray_color(r, clr, 0);
        }
      }

      set_pixel(x, y, clr * (1. / SQ(NSAMPLE)));
#else
      Ray r = ray(x + 0.5, y + 0.5);
      ray_color(r, clr, 0);
      set_pixel(x, y, clr);
#endif
    }
  }

  auto end = chrono::steady_clock::now();
  printf(".\nDone in %lld seconds.\n",
         chrono::duration_cast<chrono::seconds>(end - begin).count());
}

void
Camera::save(const string& fn) const
{
  write_rgba(fn, &this->pixels[0][0], this->pw, this->ph);
}