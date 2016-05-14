//Simple Diffuse shader
#include<ai.h>
#include <ai_sampler.h>
#include<string.h>

struct ShaderData
{
	AtSampler* diffuse_sampler;
	int GI_diffuse_samples;
};

AI_SHADER_NODE_EXPORT_METHODS(hnDiffuseMtd);

enum DiffuseParams {
	p_color,
	p_intensity,
	p_roughness
};

node_parameters
{
	AiParameterRGB("color", 1.0f, 1.0f, 1.0f);
	AiParameterFlt("intensity", 1.0f);
	AiParameterFlt("roughness", 1.0f);
}

node_initialize
{
	ShaderData* data = new ShaderData;
	AiNodeSetLocalData(node, data);
	data->diffuse_sampler = NULL;
}

node_update
{
	ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);
	AtNode *options = AiUniverseGetOptions();
	data->GI_diffuse_samples = AiNodeGetInt(options, "GI_diffuse_samples");
	data->diffuse_sampler = AiSampler(data->GI_diffuse_samples, 2);
}

node_finish
{
	if (AiNodeGetLocalData(node))
	{
		ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);
		AiSamplerDestroy(data->diffuse_sampler);
		AiNodeSetLocalData(node, NULL);
		delete data;
	}
}

shader_evaluate
{
	ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

	AtColor color = AiShaderEvalParamRGB(p_color);
	float intensity = AiShaderEvalParamFlt(p_intensity);
	float roughness = AiShaderEvalParamFlt(p_roughness);

	//evaluate direct diffuse contribution using Oren-Nayar BRDF
	void* misData = AiOrenNayarMISCreateData(sg, roughness);
	AiLightsPrepare(sg);
	AtRGB diffSample = AI_RGB_BLACK;
	AtRGB diffResult = AI_RGB_BLACK;
	float lgtDiffInt;

	//direct lighting
	while (AiLightsGetSample(sg))
	{
		lgtDiffInt = AiLightGetDiffuse(sg->Lp);
		diffSample = AiEvaluateLightSample(sg, misData, AiCookTorranceMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF)*lgtDiffInt;
		diffResult += diffSample;
	}
	sg->out.RGB = diffResult;

	//indirect diffuse
	AtRGB resultDiffuseInDirect = AI_RGB_BLACK;

	float samples[2];
	AtRay wi_ray;
	AtVector wi;
	AtScrSample scrs;
	AtVector H;

	// build a local frame for sampling
	AtVector U, V;
	AiBuildLocalFramePolar(&U, &V, &sg->N);

	//Camera ray direction in shading coordinates, omega_o
	AtVector wo = -sg->Rd;

	AtSamplerIterator* sampit = AiSamplerIterator(data->diffuse_sampler, sg);
	AiMakeRay(&wi_ray, AI_RAY_DIFFUSE, &sg->P, NULL, AI_BIG, sg);
	while (AiSamplerGetSample(sampit, samples))
	{
		// cosine hemisphere sampling as O-N sampling does not work outside of a light loop
		//Refer 13.6.2 and 13.6.3 of PBRT 2nd edition
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
			AtRGB f = AiOrenNayarMISBRDF(misData, &wi) / p;
			resultDiffuseInDirect += scrs.color * f;
		}
	}
	resultDiffuseInDirect *= AiSamplerGetSampleInvCount(sampit);
	resultDiffuseInDirect *= color*intensity;
	
	sg->out.RGB = diffResult + resultDiffuseInDirect;

}

node_loader
{
	if (i > 0)
		return false;

	node->methods = hnDiffuseMtd;
	node->output_type = AI_TYPE_RGB;
	node->name = "diffuse";
	node->node_type = AI_NODE_SHADER;
	strcpy_s(node->version, AI_VERSION);
	return true;
}