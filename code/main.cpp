#include <stdlib.h>
#include <fstream>
#include <vector>

#include "glm\glm.hpp"
using glm::vec3;
using namespace glm;

using namespace std;

#include "CIE.h"

const int IMAGE_WIDTH = 400;
const int IMAGE_HEIGHT = 400;

const int NUM_SAMPLES = 50;

const float MAX_DIST = 8000;
const float PI = 3.14159;

vec3 buffer[IMAGE_WIDTH][IMAGE_HEIGHT];

float nrand()
{
	return (float)rand() / RAND_MAX;
}

struct material
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
};

// non-normalized bell curve
float bell(float x, float m, float s)
{
	return exp(-(x - m)*(x - m) / (2 * s*s));
}

//-----------------------------------------------------------------------------
// Intersection stuff
//-----------------------------------------------------------------------------
float rPlane(vec3 p, vec3 n, vec3 ray)
{
	if (abs(dot(ray, n)) < 0.001)
		return MAX_DIST;
	float d = dot(p, n) / dot(ray, n);
	if (d < 0)
		return MAX_DIST;
	return d;
}
float rSphere(vec3 p, float rad, vec3 ray)
{
	if (dot(p, ray) < 0)
		return MAX_DIST;
	vec3 q = dot(p, ray) * ray;
	float l = length(p - q);
	if (l > rad)
		return MAX_DIST;

	float m = length(q);
	float ret = m - sqrt(rad*rad - l*l);
	if (ret < 0)
		return MAX_DIST;
	return ret;
}

float BRDF(float lambda, vec3 p, vec3 inDir, vec3 outDir)
{
	float ret;
	if (abs(p.x + 5) < 0.1)
		ret = 0.8 * bell(lambda, 600, 20);
	else if (abs(p.x - 5) < 0.1)
		ret = 0.8 * bell(lambda, 550, 20);
	else
		ret = 0.8;

	return ret / PI;
}
float emmision(vec3 p)
{
	if (abs(p.y - 5) < 0.1 && length(vec2(p.x, p.z + 15)) < 1.414)
	{
		return 50.f;
	}
	return 0.f;
}

float intersect(vec3 o, vec3 r, vec3& normal)
{
	float ret = MAX_DIST;
	float q;

	q = rPlane(vec3(0, -5, 0) - o, vec3(0, 1, 0), r);
	if (q < ret)
	{
		ret = q;
		normal = vec3(0, 1, 0);
	}
	q = rPlane(vec3(0, 5, 0) - o, vec3(0, -1, 0), r);
	if (q < ret)
	{
		ret = q;
		normal = vec3(0, -1, 0);
	}


	q = rPlane(vec3(5, 0, 0) - o, vec3(-1, 0, 0), r);
	if (q < ret)
	{
		ret = q;
		normal = vec3(-1, 0, 0);
	}
	q = rPlane(vec3(-5, 0, 0) - o, vec3(1, 0, 0), r);
	if (q < ret)
	{
		ret = q;
		normal = vec3(1, 0, 0);
	}


	q = rPlane(vec3(0, 0, -20) - o, vec3(0, 0, 1), r);
	if (q < ret)
	{
		ret = q;
		normal = vec3(0, 0, 1);
	}
	q = rPlane(vec3(0, 0, 1) - o, vec3(0, 0, -1), r);
	if (q < ret)
	{
		ret = q;
		normal = vec3(0, 0, -1);
	}


	q = rSphere(vec3(-2, -3, -17) - o, 2, r);
	if (q < ret)
	{
		ret = q;
		normal = normalize(r * q + o - vec3(-2, -3, -17));
	}

	return ret;
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

vec3 radiance(vec3 o, vec3 ray)
{
	// Decide a-priori what the wavelength of this sample will be
	// Note: this only works if all lights output the exact same spectrums
	float lambda = nrand() * 225 + 425;
	
	float accum = 1;
	while(true)
	{
		// Russian Roulette
		float r = nrand();
		float russian = min(1.f, accum);
		if (russian < r)
			break;
		
		vec3 normal;
		float dist = intersect(o, ray, normal) - 0.001;
		vec3 p = o + ray * dist;

		if (dist > 100)
			return vec3(0, 0, 0);

		vec3 emm = emmision(p) * xyz_to_rgb(wavelength_to_xyz(1.f, lambda));
		if (dot(emm, emm) > 0.001)
			return accum * emm / russian;

		vec3 nextRay = randCosineWeightedRay(normal);
		float pdf = dot(nextRay, normal) / PI;

		accum *= BRDF(lambda, p, nextRay, -ray) * max(0.f, dot(normal, nextRay)) / (pdf * russian);

		o = p;
		ray = nextRay;
	}

	return vec3();
}

int main()
{
	for (int i = 0; i < NUM_SAMPLES; ++i)
	{
		printf("Iteration %d\n", i);
		for (int x = 0; x < IMAGE_WIDTH; ++x)
		{
			for (int y = 0; y < IMAGE_HEIGHT; ++y)
			{
				vec3 ray = vec3((float)(x - IMAGE_WIDTH / 2) / IMAGE_WIDTH, (float)(y - IMAGE_HEIGHT / 2) / IMAGE_HEIGHT, -1.0);
				ray = normalize(ray);

				//buffer[x][y] = vec3((float)x / IMAGE_WIDTH, (float)y / IMAGE_HEIGHT, 0);
				buffer[x][y] += radiance(vec3(0, -1, 0), ray) / (float)NUM_SAMPLES;
			}
		}
	}

	// World's worst tonemapping
	for (int x = 0; x < IMAGE_WIDTH; ++x)
	{
		for (int y = 0; y < IMAGE_HEIGHT; ++y)
		{
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