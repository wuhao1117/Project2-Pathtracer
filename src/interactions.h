// CIS565 CUDA Raytracer: A parallel raytracer for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
// This file includes code from:
// Yining Karl Li's TAKUA Render, a massively parallel pathtracing renderer: http://www.yiningkarlli.com

#ifndef INTERACTIONS_H
#define INTERACTIONS_H

#include "intersections.h"

struct Fresnel {
  float reflectionCoefficient;
  float transmissionCoefficient;
};

struct AbsorptionAndScatteringProperties{
    glm::vec3 absorptionCoefficient;
    float reducedScatteringCoefficient;
};

//forward declaration
__host__ __device__ bool calculateScatterAndAbsorption(ray& r, float& depth, AbsorptionAndScatteringProperties& currentAbsorptionAndScattering, glm::vec3& unabsorbedColor, material m, float randomFloatForScatteringDistance, float randomFloat2, float randomFloat3);
__host__ __device__ glm::vec3 getRandomDirectionInSphere(float xi1, float xi2);
__host__ __device__ glm::vec3 calculateTransmission(glm::vec3 absorptionCoefficient, float distance);
__host__ __device__ glm::vec3 calculateTransmissionDirection(const glm::vec3 &incidentDir, const glm::vec3 &surfaceNormal, const float incidentIOR, const float transmittedIOR, float &nextIndexOfRefraction);
__host__ __device__ glm::vec3 calculateReflectionDirection(const glm::vec3 &incidentDir, const glm::vec3 &surfaceNormal);
__host__ __device__ Fresnel calculateFresnel(glm::vec3 normal, glm::vec3 incident, float incidentIOR, float transmittedIOR, glm::vec3 reflectionDirection, glm::vec3 transmissionDirection);
__host__ __device__ glm::vec3 calculateRandomDirectionInHemisphere(glm::vec3 normal, float xi1, float xi2);

//TODO (OPTIONAL): IMPLEMENT THIS FUNCTION
__host__ __device__ glm::vec3 calculateTransmission(glm::vec3 absorptionCoefficient, float distance) {
    return glm::vec3(0,0,0);
}

//TODO (OPTIONAL): IMPLEMENT THIS FUNCTION
__host__ __device__ bool calculateScatterAndAbsorption(ray& r, float& depth, AbsorptionAndScatteringProperties& currentAbsorptionAndScattering,
                                                        glm::vec3& unabsorbedColor, material m, float randomFloatForScatteringDistance, float randomFloat2, float randomFloat3){
    return false;
}

//TODO (OPTIONAL): IMPLEMENT THIS FUNCTION
__host__ __device__ Fresnel calculateFresnel(glm::vec3 surfaceNormal, glm::vec3 incidentDir, float incidentIOR, float transmittedIOR, glm::vec3 reflectionDirection, glm::vec3 transmissionDirection) {
  
	Fresnel fresnel;

//    glm::vec3 normal = glm::normalize(surfaceNormal);
    glm::vec3 inDir = glm::normalize(incidentDir);

    float cosIncidentAngle = glm::dot(inDir, -surfaceNormal);
	float sinIncidentAngle = glm::length(glm::cross(inDir, surfaceNormal));
	float cosTransmitAngle;

	if(incidentIOR < transmittedIOR) // from air to glass
	{
		cosTransmitAngle = cos(asin(incidentIOR / transmittedIOR * sinIncidentAngle));
		fresnel.reflectionCoefficient  = pow(abs((incidentIOR * cosIncidentAngle - transmittedIOR * cosTransmitAngle) 
			                                   / (incidentIOR * cosIncidentAngle + transmittedIOR * cosTransmitAngle)), 2);
		fresnel.reflectionCoefficient += pow(abs((incidentIOR * cosTransmitAngle - transmittedIOR * cosIncidentAngle) 
			                                   / (incidentIOR * cosTransmitAngle + transmittedIOR * cosIncidentAngle)), 2);
		fresnel.reflectionCoefficient /= 2.0f;
	}
	else                                    // from glass to air
	{
		float sinCriticalAngle = transmittedIOR / incidentIOR;
		if(sinIncidentAngle > sinCriticalAngle) 
		{
			fresnel.reflectionCoefficient = 1.0f;
		}
		else
		{
			cosTransmitAngle = cos(asin(incidentIOR / transmittedIOR * sinIncidentAngle));
			fresnel.reflectionCoefficient  = pow(abs((incidentIOR * cosIncidentAngle - transmittedIOR * cosTransmitAngle) 
				/ (incidentIOR * cosIncidentAngle + transmittedIOR * cosTransmitAngle)), 2);
			fresnel.reflectionCoefficient += pow(abs((incidentIOR * cosTransmitAngle - transmittedIOR * cosIncidentAngle) 
				/ (incidentIOR * cosTransmitAngle + transmittedIOR * cosIncidentAngle)), 2);
			fresnel.reflectionCoefficient /= 2.0f;
		}
		
	}

    fresnel.transmissionCoefficient = 1 - fresnel.reflectionCoefficient;
    return fresnel;
}

//LOOK: This function demonstrates cosine weighted random direction generation in a sphere!
__host__ __device__ glm::vec3 calculateRandomDirectionInHemisphere(glm::vec3 normal, float xi1, float xi2) {
    
    //crucial difference between this and calculateRandomDirectionInSphere: THIS IS COSINE WEIGHTED!
    
    float up = sqrt(xi1); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = xi2 * TWO_PI;
    
    //Find a direction that is not the normal based off of whether or not the normal's components are all equal to sqrt(1/3) or whether or not at least one component is less than sqrt(1/3). Learned this trick from Peter Kutz.
    
    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
      directionNotNormal = glm::vec3(1, 0, 0);
    } else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
      directionNotNormal = glm::vec3(0, 1, 0);
    } else {
      directionNotNormal = glm::vec3(0, 0, 1);
    }
    
    //Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 = glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 = glm::normalize(glm::cross(normal, perpendicularDirection1));
    
    return ( up * normal ) + ( cos(around) * over * perpendicularDirection1 ) + ( sin(around) * over * perpendicularDirection2 );
    
}

//TODO: IMPLEMENT THIS FUNCTION
//Now that you know how cosine weighted direction generation works, try implementing non-cosine (uniform) weighted random direction generation.
//This should be much easier than if you had to implement calculateRandomDirectionInHemisphere.
__host__ __device__ glm::vec3 getRandomDirectionInSphere(float xi1, float xi2) {

    return glm::vec3(0,0,0);
}

//TODO (PARTIALLY OPTIONAL): IMPLEMENT THIS FUNCTION
//returns 0 if diffuse scatter, 1 if reflected, 2 if transmitted.
__host__ __device__ int calculateBSDF(ray& r, glm::vec3 intersect, glm::vec3 normal, glm::vec3 emittedColor,
                                       AbsorptionAndScatteringProperties& currentAbsorptionAndScattering,
                                       glm::vec3& color, glm::vec3& unabsorbedColor, material m){

    return 1;
};

//TODO (OPTIONAL): IMPLEMENT THIS FUNCTION
__host__ __device__ glm::vec3 calculateReflectionDirection(const glm::vec3 &incidentDir, const glm::vec3 &surfaceNormal)
{
	return incidentDir - 2.0f * surfaceNormal * glm::dot(incidentDir, surfaceNormal);
}

//TODO (OPTIONAL): IMPLEMENT THIS FUNCTION
__host__ __device__ glm::vec3 calculateTransmissionDirection(const glm::vec3 &incidentDir, const glm::vec3 &surfaceNormal, const float refractionIndex1, const float refractionIndex2, float &nextIndexOfRefraction)
{
	glm::vec3 inDir = glm::normalize(incidentDir);
	glm::vec3 refDirection = calculateReflectionDirection(inDir, surfaceNormal);	
	glm::vec3 refraDirection;
	float sinIncidentAngle = glm::length(glm::cross(inDir, surfaceNormal));
	float sinCriticalAngle = refractionIndex2 / refractionIndex1;

	if(refractionIndex1 > refractionIndex2 && sinIncidentAngle > sinCriticalAngle) //total internal reflection
	{
		refraDirection =  refDirection;       
		nextIndexOfRefraction = refractionIndex1;
	}
	else                     
	{
		float transmitAngle = asin(refractionIndex1 / refractionIndex2 * sinIncidentAngle);
		glm::vec3 tangentVec = inDir + refDirection;
		refraDirection = tangentVec - glm::length(tangentVec) / tan(transmitAngle) * surfaceNormal;
		nextIndexOfRefraction = refractionIndex2;
	}
	return glm::normalize(refraDirection);
}

#endif
