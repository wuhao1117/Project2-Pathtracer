// CIS565 CUDA Raytracer: A parallel raytracer for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
// This file includes code from:
//       Yining Karl Li's TAKUA Render, a massively parallel pathtracing renderer: http://www.yiningkarlli.com

#ifndef CUDASTRUCTS_H
#define CUDASTRUCTS_H

#include "glm/glm.hpp"
#include "cudaMat4.h"
#include <cuda_runtime.h>
#include <string>

enum GEOMTYPE{ SPHERE, CUBE, MESH };

struct ray {
	glm::vec3 origin;
	glm::vec3 direction;
	glm::vec3 tempColor;
	int index;
	float mediaIOR;
};

struct geom {// for moving geometry
	enum GEOMTYPE type;
	int materialid;
	int frames;
	glm::vec3* translations;
	glm::vec3* rotations;
	glm::vec3* scales;
	cudaMat4* transforms;
	cudaMat4* inverseTransforms;

	int vertexCount;
	glm::vec3* vertexList;
	int faceCount;
	glm::vec3* faceList;

	glm::vec3 boundingBoxMin;
	glm::vec3 boundingBoxMax;
	
};

struct staticGeom {// for static geometry
	enum GEOMTYPE type;
	int materialid;
	glm::vec3 translation;
	glm::vec3 rotation;
	glm::vec3 scale;
	cudaMat4 transform;
	cudaMat4 inverseTransform;

	int vertexCount;
	glm::vec3* vertexList;
	int faceCount;
	glm::vec3* faceList;

	glm::vec3 boundingBoxMin;
	glm::vec3 boundingBoxMax;
};

struct cameraData {// for static camera
	glm::vec2 resolution;
	glm::vec3 position;
	glm::vec3 view;
	glm::vec3 up;
	glm::vec2 fov;
	float focalLength;
	float aperture;
};

struct camera {// for moving camera
	glm::vec2 resolution;
	glm::vec3* positions;
	glm::vec3* views;
	glm::vec3* ups;
	int frames;
	glm::vec2 fov;
	float focalLength;
	float aperture;
	unsigned int iterations;
	glm::vec3* image;
	ray* rayList;
	std::string imageName;
};

struct material{
	glm::vec3 color;
	float specularExponent;
	glm::vec3 specularColor; // What is specular color?
	float hasReflective;
	float hasRefractive;
	float indexOfRefraction;
	float hasScatter;        // How to incorporate scattering?
	glm::vec3 absorptionCoefficient;
	float reducedScatterCoefficient;
	float emittance;         // Used as light source
};

#endif //CUDASTRUCTS_H
