#include <iostream>
#include <ai_sampler.h>
#include <ai.h>

#include "alUtil.h"
#include "MIS.h"
#include "alSurface.h"

AI_SHADER_NODE_EXPORT_METHODS(ahDiffuseSurfaceMtd)

typedef float AtFloat;

inline void flipNormals(AtShaderGlobals* sg)
{
	sg->Nf = -sg->Nf;
	sg->Ngf = -sg->Ngf;
}

enum ahDiffuseParams
{
	// diffuse
	p_diffuseStrength = 0,
	p_diffuseColor,
	p_diffuseRoughness,
	p_diffuseExtraSamples
};

node_parameters
{
	AiParameterFLT("diffuseStrength", 1.0f);
	AiParameterRGB("diffuseColor", 0.18f, 0.18f, 0.18f);
	AiParameterFLT("diffuseRoughness", 0.0f);
	AiParameterINT("diffuseExtraSamples", 0);
}

#ifdef MSVC
#define _CRT_SECURE_NO_WARNINGS 1
#endif
node_loader
{
	if (i > 0) return 0;
	node->methods = ahDiffuseSurfaceMtd;
	node->output_type = AI_TYPE_RGB;
	node->name = "ahDiffuse";
	node->node_type = AI_NODE_SHADER;
	strcpy_s(node->version, AI_VERSION);
	return true;
}

node_initialize
{
	ShaderData* data = new ShaderData;
	AiNodeSetLocalData(node, data); //Store custom data in the node
	data->diffuse_sampler = NULL;
};

node_finish
{
	if (AiNodeGetLocalData(node))
	{
		ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

		AiSamplerDestroy(data->diffuse_sampler);

		AiNodeSetLocalData(node, NULL);
		delete data;
	}
};

node_update
{
	ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);

	// store some options we'll reuse later
	AtNode *options = AiUniverseGetOptions();
	data->GI_diffuse_depth = AiNodeGetInt(options, "GI_diffuse_depth");
	data->GI_diffuse_samples = AiNodeGetInt(options, "GI_diffuse_samples") + params[p_diffuseExtraSamples].INT;
	data->diffuse_samples2 = SQR(data->GI_diffuse_samples);

	// setup samples
	AiSamplerDestroy(data->diffuse_sampler);
	data->diffuse_sampler = AiSampler(data->GI_diffuse_samples, 2);
};

shader_evaluate
{
	ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);
	
	// Initialize parameter temporaries
	// TODO: reorganize this so we're not evaluating upstream when we don't need the parameters, e.g. in shadow rays
	AtRGB diffuseColor = AiShaderEvalParamRGB(p_diffuseColor) * AiShaderEvalParamFlt(p_diffuseStrength);
	float diffuseRoughness = AiShaderEvalParamFlt(p_diffuseRoughness);


	// Initialize result temporaries
	AtRGB result_diffuseDirect = AI_RGB_BLACK;
	AtRGB result_diffuseDirectRaw = AI_RGB_BLACK;
	AtRGB result_diffuseIndirect = AI_RGB_BLACK;
	AtRGB result_diffuseIndirectRaw = AI_RGB_BLACK;

	bool do_diffuse = true;
	int diffuse_samples = data->GI_diffuse_samples;

	// build a local frame for sampling
	AtVector U, V;
	AiBuildLocalFramePolar(&U, &V, &sg->N);

	// View direction, omega_o
	AtVector wo = -sg->Rd;

	void* dmis = AiOrenNayarMISCreateData(sg, diffuseRoughness);

	//direct diffuse
	AiLightsPrepare(sg);
	while (AiLightsGetSample(sg))
	{
		float diffuse_strength = AiLightGetDiffuse(sg->Lp);
		result_diffuseDirect +=
				AiEvaluateLightSample(sg, dmis, AiOrenNayarMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF)
				* diffuse_strength;
	}
	



	// Multiply by the colors
	result_diffuseDirectRaw = result_diffuseDirect;
	result_diffuseDirect *= diffuseColor;

	// Sample BRDFS
	float samples[2];
	AtRay wi_ray;
	AtVector wi;
	AtScrSample scrs;

	AtSamplerIterator* sampit = AiSamplerIterator(data->diffuse_sampler, sg);
	AiMakeRay(&wi_ray, AI_RAY_DIFFUSE, &sg->P, NULL, AI_BIG, sg);
	while (AiSamplerGetSample(sampit, samples))
	{
		//cosine hemisphere sampling
		float stheta = sqrtf(float(samples[0]));
		float phi = float(AI_PITIMES2 * samples[1]);
		wi.x = stheta * cosf(phi);
		wi.y = stheta * sinf(phi);
		wi.z = sqrtf(1.0f - float(samples[0]));
		AiV3RotateToFrame(wi, U, V, sg->Nf);

		float cos_theta = AiV3Dot(wi, sg->Nf);
		if (cos_theta <= 0.0f) continue;

		float p = cos_theta * float(AI_ONEOVERPI);

		// trace the ray
		wi_ray.dir = wi;
		if (AiTrace(&wi_ray, &scrs))
		{
			AtRGB f = AiOrenNayarMISBRDF(dmis, &wi) / p;
			result_diffuseIndirectRaw += scrs.color * f;
		}
	}
	result_diffuseIndirectRaw *= AiSamplerGetSampleInvCount(sampit);
	result_diffuseIndirect = result_diffuseIndirectRaw * diffuseColor;


sg->out.RGB = result_diffuseDirect
	+ result_diffuseIndirect;
}