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

const int NUM_SAMPLES = 10;

const float MAX_DIST = 8000;
const float PI = 3.14159;

vec3 buffer[IMAGE_WIDTH][IMAGE_HEIGHT];
RTCScene scene;

float nrand()
{
	return (float)rand() / RAND_MAX;
}

/*struct material
{
	float mean = 500;
	float std = 100;

	material(float m, float s)
		: mean(m), std(s)
	{}

	material()
		: material(0, 0)
	{}
};

struct light_path_node
{
	vec3 normal;
	vec3 ray;
	material m;
	float weight;

	light_path_node() {}
};*/

// non-normalized bell curve
float bell(float x, float m, float s)
{
	return exp(-(x - m)*(x - m) / (2 * s*s));
}

//-----------------------------------------------------------------------------
// Intersection stuff
//-----------------------------------------------------------------------------
float BRDF(float lambda, vec3 p, vec3 inDir, vec3 outDir)
{
	return 0.4 / PI;
}
float emmision(float lambda, vec3 p)
{
	return 0.f;
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
	//material* mat;
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
}

//-----------------------------------------------------------------------------
// Coloring and ray tracing stuff
//-----------------------------------------------------------------------------
vec3 getTangent(vec3 norm)
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
vec3 randCosineWeightedRay(vec3 norm)
{
	float rx = 1, rz = 1;
	while (rx*rx + rz*rz >= 1)
	{
		rx = 2 * nrand() - 1.0f;
		rz = 2 * nrand() - 1.0f;
	}
	float ry = sqrt(1 - rx*rx - rz*rz);

	vec3 tangent = getTangent(norm);
	vec3 bitangent = cross(norm, tangent);

	vec3 castRay = normalize(tangent*rx + bitangent*rz + norm*ry);
	return castRay;
}

float radiance(float lambda, vec3 o, vec3 ray)
{
	float accum = 1;
	while(true)
	{
		// Russian Roulette
		float r = nrand();
		float russian = min(1.f, accum);
		if (russian < r)
			break;

		intersection_info info;
		get_intersection_info(o, ray, &info);
		
		vec3 normal = info.normal;
		vec3 p = info.pos;

		if (info.t < -0.1)
			return 10.f * accum / russian;

		float emm = emmision(lambda, p);
		if (dot(emm, emm) > 0.001)
			return accum * emm / russian;

		vec3 nextRay = randCosineWeightedRay(normal);
		float pdf = dot(nextRay, normal) / PI;

		accum *= BRDF(lambda, p, nextRay, -ray) * max(0.f, dot(normal, nextRay)) / (pdf * russian);

		o = p;
		ray = nextRay;
	}

	return 0;
}

int main()
{
	RTCDevice device = rtcNewDevice(NULL);
	scene = rtcDeviceNewScene(device, RTC_SCENE_STATIC, RTC_INTERSECT1);

	addObj(scene, "models/deccer2.obj", vec3(0, 0, 0));

	rtcCommit(scene);
	
	for (int i = 0; i < NUM_SAMPLES; ++i)
	{
		printf("Iteration %d\n", i);
		for (int x = 0; x < IMAGE_WIDTH; ++x)
		{
			for (int y = 0; y < IMAGE_HEIGHT; ++y)
			{
				vec3 ray = vec3((float)(x - IMAGE_WIDTH / 2) / IMAGE_WIDTH, (float)(y - IMAGE_HEIGHT / 2) / IMAGE_HEIGHT, -1.0);
				ray = normalize(ray);
				vec3 o = vec3(0, 1, 2.9);

				float lambda = nrand() * 440 + 390;
				buffer[x][y] += xyz_to_rgb(wavelength_to_xyz(lambda)) * radiance(lambda, o, ray) / (float)NUM_SAMPLES;
			}
		}
	}

	// World's worst tonemapping
	for (int x = 0; x < IMAGE_WIDTH; ++x)
	{
		for (int y = 0; y < IMAGE_HEIGHT; ++y)
		{
			//buffer[x][y] = xyz_to_rgb(buffer[x][y]);
			buffer[x][y] /= buffer[x][y] + vec3(1, 1, 1);
		}
	}

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
	//getchar();
}