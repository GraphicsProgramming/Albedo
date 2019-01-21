#include <stdlib.h>
#include <fstream>
#include <vector>
using namespace std;

#include "glm\glm.hpp"
using glm::vec3;
using namespace glm;

#include <embree2\rtcore.h>
#include <embree2\rtcore_ray.h>

#include "CIE.h"
#include "model.h"

const int IMAGE_WIDTH = 400;
const int IMAGE_HEIGHT = 400;

const int NUM_SAMPLES = 100;

const float MAX_DIST = 8000;
const float PI = 3.14159;

vec3 buffer[IMAGE_WIDTH][IMAGE_HEIGHT];
RTCScene scene;

float nrand()
{
	return (float)rand() / RAND_MAX;
}

// non-normalized bell curve
float bell(float x, float m, float s)
{
	return exp(-(x - m)*(x - m) / (2 * s*s));
}
float pdf_gaussian(float x, float m, float s)
{
	return bell(x, m, s) / (s * sqrt(2 * PI));
}
float cdf_gaussian(float x, float m, float s)
{
	return erfc(-(x - m) / (s * sqrt(2))) / 2;
}
double gaussian_rand(double m, double s)
{
	double u1, u2;
	do
	{
		u1 = nrand();
		u2 = nrand();
	}
	while ( u1 <= 0.001 );

	float z0 = sqrt(-2.0 * log(u1)) * cos(2 * PI * u2);
	return z0 * s + m;
}

//-----------------------------------------------------------------------------
// Intersection stuff
//-----------------------------------------------------------------------------
float BRDF(float lambda, material m, vec3 inDir, vec3 outDir)
{
	return m.diffuse_albedo * bell(lambda, m.diffuse_mean, m.diffuse_stddev) / PI;
}
float emmision(float lambda, material mat)
{
	return mat.light_intensity;
}

RTCRay make_ray(vec3 o, vec3 dir)
{
	RTCRay ray;
	ray.org[0] = o.x; ray.org[1] = o.y; ray.org[2] = o.z;
	ray.dir[0] = dir.x; ray.dir[1] = dir.y; ray.dir[2] = dir.z;
	ray.tnear = 0;
	ray.tfar = MAX_DIST;
	ray.geomID = RTC_INVALID_GEOMETRY_ID;
	return ray;
}

struct intersection_info
{
	float t;
	vec2 uv;
	vec3 pos;
	vec3 normal;
	material mat;
};
void get_intersection_info(vec3 o, vec3 ray, intersection_info* ret)
{
	RTCRay rtcNegODir = make_ray(o, ray);
	rtcIntersect(scene, rtcNegODir);

	if (rtcNegODir.geomID == -1)
	{
		ret->t = -1;
		return;
	}

	ret->t = rtcNegODir.tfar - 0.001f;

	ret->pos = o + ray * ret->t;
	ret->normal = normalize(vec3(-rtcNegODir.Ng[0], -rtcNegODir.Ng[1], -rtcNegODir.Ng[2]));
	if (dot(ret->normal, ray * -1.0f) < 0)
		ret->normal *= -1.0f;
	
	model m = models[rtcNegODir.geomID];
	ret->mat = m.mat;
}

//-----------------------------------------------------------------------------
// Coloring and ray tracing stuff
//-----------------------------------------------------------------------------
// returns any vector perpendicular to the norm (or tangent to surface)
vec3 get_tangent(vec3 norm)
{
	vec3 tangent;
	vec3 c1 = cross(norm, vec3(0, 0, 1));
	vec3 c2 = cross(norm, vec3(0, 1, 0));
	if (dot(c1, c1) > dot(c2, c2))
		tangent = c1;
	else
		tangent = c2;
	return tangent;
}
vec3 rand_cosine_weighted_ray(vec3 norm)
{
	float rx = 1, rz = 1;
	while (rx*rx + rz*rz >= 1)
	{
		rx = 2 * nrand() - 1.0f;
		rz = 2 * nrand() - 1.0f;
	}
	float ry = sqrt(1 - rx*rx - rz*rz);

	vec3 tangent = get_tangent(norm);
	vec3 bitangent = cross(norm, tangent);

	vec3 castRay = normalize(tangent*rx + bitangent*rz + norm*ry);
	return castRay;
}
vec3 rand_hemisphere_ray(vec3 norm)
{
	float rx = 1, ry = 1, rz = 1;
	while (rx*rx + ry*ry + rz*rz >= 1)
	{
		rx = 2 * nrand() - 1;
		ry = 2 * nrand() - 1;
		rz = 2 * nrand() - 1;
	}
	vec3 ray = vec3(rx, ry, rz);

	ray = normalize(ray);
	if (dot(ray, norm) < 0)
		ray *= -1;

	return ray;
}

struct light_path_node
{
	vec3 pos;
	vec3 normal;
	vec3 ray_towards_camera;
	vec3 ray_towards_light;
	material mat;

	// thing to multiply against emmision to get total weight
	// includes this vert's probability, but not thie vert's BRDF and projected area component
	float accumulated_weight;

	light_path_node() {}
};

// only selects lights now
void construct_light_path(float lambda, vector<light_path_node>& path)
{
	vec3 o;
	vec3 ray;
	
	// Select a triangle
	{
		light_triangle t;
		float r = nrand() * total_light_area;
		for (int i = 0; i < light_triangles.size(); ++i)
		{
			if (r > light_triangles[i].area && i != light_triangles.size() - 1)
			{
				r -= light_triangles[i].area;
				continue;
			}

			t = light_triangles[i];
			break;
		}

		float r1 = nrand();
		float r2 = nrand();
		o = t.p0 + r1 * (t.p1 - t.p0) + r2 * (t.p2 - t.p0);

		light_path_node n;
		n.pos = o;
		n.normal = normalize(cross(t.p1 - t.p0, t.p2 - t.p0));
		//n.ray_towards_camera = rand_hemisphere_ray(n.normal);
		n.mat = models[t.model_id].mat;
		n.accumulated_weight = emmision(lambda, n.mat) * total_light_area;
		path.push_back(n);

		ray = n.ray_towards_camera;
	}
}
void construct_camera_path(float lambda, vec3 o, vec3 ray, vector<light_path_node>& path)
{
	// add origin
	{
		light_path_node n;
		n.pos = o;
		n.normal = ray;
		n.ray_towards_light = ray;
		n.accumulated_weight = 1;

		path.push_back(n);
	}

	float accumulated_weight = 1;
	while (true)
	{
		// Intersection
		intersection_info info;
		get_intersection_info(o, ray, &info);
		if (info.t < -0.1)
			break;

		vec3 p = info.pos;
		vec3 normal = info.normal;

		light_path_node n;
		n.pos = p;
		n.normal = normal;
		n.ray_towards_camera = -ray;
		n.ray_towards_light = vec3(0);
		n.mat = info.mat;
		n.accumulated_weight = accumulated_weight;
		path.push_back(n);
		light_path_node* cur_n = &(path[path.size() - 1]); // we still need to modify this node a bit

		float emm = emmision(lambda, info.mat);
		if (dot(emm, emm) > 0.001)
			break;
		
		vec3 nextRay = rand_cosine_weighted_ray(normal);
		float weight = BRDF(lambda, n.mat, nextRay, -ray) * PI;
		
		cur_n->ray_towards_light = nextRay;
		accumulated_weight *= weight;

		// Store data for next iteration
		o = p;
		ray = nextRay;

		// Russian Roulette
		// todo: weight this also by percieved brightness of lambda for good measure
		float r = nrand();
		float russian = min(1.f, accumulated_weight);
		if (russian < r)
			break;
		
		accumulated_weight /= russian;
	}
}

float radiance(float lambda, vec3 o, vec3 ray)
{
	vector<light_path_node> camera_path;
	camera_path.reserve(10);
	construct_camera_path(lambda, o, ray, camera_path);

	light_path_node last_node = camera_path.back();

	// if we hit a light by chance, just accumulate it
	float emm = emmision(lambda, last_node.mat);
	if (emm > 0.01)
		return emm * last_node.accumulated_weight;

	if (camera_path.size() == 1)
		return 0; // it escaped to infinity too early to preempt it

	// so the path didn't hit a light. now let's append a light sample to the path
	vector<light_path_node> light_path;
	construct_light_path(lambda, light_path);
	light_path_node light_node = light_path.front();

	// make sure there's line of sight to the selected light sample
	vec3 light_ray = light_node.pos - last_node.pos;
	vec3 light_ray_norm = normalize(light_ray);
	intersection_info info;
	get_intersection_info(last_node.pos, light_ray_norm, &info);

	if (abs(info.t - length(light_ray)) < 0.01)
	{
		float camera_weight = last_node.accumulated_weight;
		float light_weight = light_node.accumulated_weight;
		float brdf = BRDF(lambda, last_node.mat, last_node.ray_towards_camera, light_ray_norm);
		return brdf * camera_weight * light_weight *
			max(0.f, dot(last_node.normal, light_ray_norm) * dot(light_node.normal, light_ray_norm)) / 
			dot(light_ray, light_ray);
	}

	// just give up at this point
	return 0;
}

int main()
{
	RTCDevice device = rtcNewDevice(NULL);
	scene = rtcDeviceNewScene(device, RTC_SCENE_STATIC, RTC_INTERSECT1);

	addObj(scene, "models/GP.obj", vec3(0, 0, 0));

	rtcCommit(scene);
	
	for (int i = 0; i < NUM_SAMPLES; ++i)
	{
		if (i % 10 == 0)
			printf("Iteration %d\n", i);
		
		for (int x = 0; x < IMAGE_WIDTH; ++x)
		{
			for (int y = 0; y < IMAGE_HEIGHT; ++y)
			{
				vec3 ray = vec3((float)(x - IMAGE_WIDTH / 2) / IMAGE_WIDTH, (float)(y - IMAGE_HEIGHT / 2) / IMAGE_HEIGHT, -1.0);
				ray = normalize(ray);
				vec3 o = vec3(0, 1, 2.9);

#if 0
				float lambda = nrand() * 440 + 390;
				float lambda_prob = 1.0 / 440.0;
#else
				// less efficient, but lower variance
				intersection_info info;
				get_intersection_info(o, ray, &info);
				if (info.t < -0.1)
					continue;
				
				// generate random lambda according to diffuse response
				float lambda = 0;
				material mat = info.mat;
				do
				{
					lambda = gaussian_rand(mat.diffuse_mean, mat.diffuse_stddev);
				} while (lambda < 390 || 830 < lambda);
				float scale = cdf_gaussian(830, mat.diffuse_mean, mat.diffuse_stddev) - 
					cdf_gaussian(390, mat.diffuse_mean, mat.diffuse_stddev);
				float lambda_prob = pdf_gaussian(lambda, mat.diffuse_mean, mat.diffuse_stddev) / scale;
#endif

				float cur_radiance = radiance(lambda, o, ray) / lambda_prob;
				buffer[x][y] += cur_radiance * wavelength_to_xyz(lambda) / (float)NUM_SAMPLES;
			}
		}
	}

	// World's worst tonemapping
	for (int x = 0; x < IMAGE_WIDTH; ++x)
	{
		for (int y = 0; y < IMAGE_HEIGHT; ++y)
		{
			buffer[x][y] = xyz_to_rgb(buffer[x][y]);
			buffer[x][y] /= buffer[x][y] + vec3(1, 1, 1);
		}
	}

	// PPM file
	ofstream file("image.ppm");
	file << "P3 " << IMAGE_WIDTH << " " << IMAGE_HEIGHT << " 255" << endl;

	for (int y = IMAGE_HEIGHT - 1; y >= 0; --y)
	{
		for (int x = 0; x < IMAGE_WIDTH; ++x)
		{
			if ((int)(buffer[x][y].x * 255) == 0x80000000 ||
				(int)(buffer[x][y].y * 255) == 0x80000000 ||
				(int)(buffer[x][y].z * 255) == 0x80000000)
			{
				//cout << "a divide by zero happened" << endl;
				/*for (int i = 0; i < 50; ++i)
				file << "\n";*/
				file << 0 << " " << 0 << " " << 0 << " ";
			}
			else
				file << (int)(buffer[x][y].x * 255) << " " << (int)(buffer[x][y].y * 255) << " " << (int)(buffer[x][y].z * 255) << " ";
		}
	}

	file.close();

	printf("Finished\n");
}