#include<ai.h>
#include<iostream>
#include<ai_sampler.h>

struct CustomData
{
	AtSampler* diffuseSampler;
};
AI_SHADER_NODE_EXPORT_METHODS(ahDiffuseSurfaceMtd)

typedef float AtFloat;

inline void flipNormals(AtShaderGlobals* sg)
{
	sg->Nf = -sg->Nf;
	sg->Ngf = -sg->Ngf;
}

enum ahDiffuseParams
{
	id_strength = 0,
	id_color,
	id_roughness
};

node_parameters
{
	AiParameterFLT("diffuseStrength", 1.0f);
	AiParameterRGB("diffuseColor", 0.18f, 0.18f, 0.18f);
	AiParameterFLT("diffuseRoughness", 0.0f);
}

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
	CustomData *customData = new CustomData;
	AiNodeSetLocalData(node, customData);
	customData->diffuseSampler = NULL;
};

node_finish
{
	if (AiNodeGetLocalData(node))
	{
		CustomData* customData = (CustomData*)AiNodeGetLocalData(node);
		AiSamplerDestroy(customData->diffuseSampler);

		//AiNodeSetLocalData(node, NULL);
		//delete customData;
	}
};

node_update
{
	CustomData* customData = (CustomData*)AiNodeGetLocalData(node);
	AiSamplerDestroy(customData->diffuseSampler);
	customData->diffuseSampler = AiSampler(36, 2);
};

shader_evaluate
{
	CustomData* customData = (CustomData*)AiNodeGetLocalData(node);

	AtRGB diffuseColor = AiShaderEvalParamRGB(id_color)* AiShaderEvalParamFlt(id_strength);;
	float roughness = AiShaderEvalParamFlt(id_roughness);

	// Initialize result temporaries
	AtRGB result_diffuseDirect = AI_RGB_BLACK;
	AtRGB result_diffuseDirectRaw = AI_RGB_BLACK;
	AtRGB result_diffuseIndirect = AI_RGB_BLACK;
	AtRGB result_diffuseIndirectRaw = AI_RGB_BLACK;

	bool do_diffuse = true;
	// build a local frame for sampling
	AtVector U, V;
	AiBuildLocalFramePolar(&U, &V, &sg->N);

	void* diffuseMISData = AiOrenNayarMISCreateData(sg, roughness);

	//direct diffuse
	AiLightsPrepare(sg);
	AtRGB LdiffuseDirect;


	if (sg->Rt & AI_RAY_CAMERA)
	{
		while (AiLightsGetSample(sg))
		{
			result_diffuseDirect += AiEvaluateLightSample(sg, diffuseMISData, AiOrenNayarMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF);
			//LdiffuseDirect = AiEvaluateLightSample(sg, diffuseMISData, AiOrenNayarMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF);
			//result_diffuseDirect += LdiffuseDirect*diffuseColor;
		}
		result_diffuseDirect *= diffuseColor;
	}



	//TODO indirect diffuse

	float samples[2];
	AtRay wi_ray;
	AtVector wi;
	AtScrSample scrs;
	AtSampler* diffuseSampler = customData->diffuseSampler;

	if (do_diffuse)
	{
		AtSamplerIterator* sampit = AiSamplerIterator(diffuseSampler, sg);
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
				AtRGB f = AiOrenNayarMISBRDF(diffuseMISData, &wi) / p;
				result_diffuseIndirectRaw += scrs.color * f;
			}
		}
		result_diffuseIndirectRaw *= AiSamplerGetSampleInvCount(sampit);
		result_diffuseIndirect = result_diffuseIndirectRaw * diffuseColor;
	}


	sg->out.RGB = result_diffuseDirect
		+ result_diffuseIndirect;
}