#include <ai.h>
#include <ai_sampler.h>
#include <string.h>

struct ShaderData
{
	AtSampler* specular_sampler;
	int GI_specular_samples;
};

AI_SHADER_NODE_EXPORT_METHODS(hnSpecularMtd);

enum hnSpecularParams
{
	p_color,
	p_intensity,
	p_roughness
};

node_parameters
{
	AiParameterRGB("color", 1.0f,1.0f,1.0f);
	AiParameterFlt("intensity", 1.0f, 1.0f, 1.0f);
	AiParameterFlt("roughness", 1.0f);
}

node_initialize
{
	ShaderData* data = new ShaderData;
	AiNodeSetLocalData(node, data);
	data->specular_sampler = NULL;
};

node_finish{
	if (AiNodeGetLocalData(node))
	{
		ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);
		AiSamplerDestroy(data->specular_sampler);
		AiNodeSetLocalData(node, NULL);
		delete data;
	}
}

node_update
{
	ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);
	AtNode *options = AiUniverseGetOptions();
	data->GI_specular_samples = AiNodeGetInt(options, "GI_specular_samples");
	data->specular_sampler = AiSampler(data->GI_specular_samples, 2);
}

shader_evaluate
{
	ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

	AtColor color = AiShaderEvalParamRGB(p_color);
	float intensity = AiShaderEvalParamFlt(p_intensity);
	float roughness = AiShaderEvalParamFlt(p_roughness);

	//build a local frame for sampling
	AtVector U, V;
	AiBuildLocalFramePolar(&U, &V, &sg->N);

	//evaluate direct specular contribution using Cook Torrance brdf
	void* misData = AiCookTorranceMISCreateData(sg, &U, &V, roughness, roughness);
	AiLightsPrepare(sg);
	AtRGB specSample = AI_RGB_BLACK;
	AtRGB specResult = AI_RGB_BLACK;
	float lgtSpecInt;

	while (AiLightsGetSample(sg))
	{
		lgtSpecInt = AiLightGetSpecular(sg->Lp);
		specSample = AiEvaluateLightSample(sg, misData, AiCookTorranceMISSample, AiCookTorranceMISBRDF, AiCookTorranceMISPDF);
		specResult += specSample*lgtSpecInt;
	}

	specResult *= color*intensity;

	//initialise specular
	AtRGB resultSpecularInDirect = AI_RGB_BLACK;
	float samples[2];
	AtRay wi_ray;
	AtVector wi;
	AtScrSample scrs;
	AtVector H;

	//Camera ray direction in shading coordinates, omega_o
	AtVector wo = -sg->Rd;

	//indirect specular
	AtSamplerIterator* sampit = AiSamplerIterator(data->specular_sampler, sg);
	AiMakeRay(&wi_ray, AI_RAY_GLOSSY, &sg->P, NULL, AI_BIG, sg);
	while (AiSamplerGetSample(sampit, samples))
	{
		wi = AiCookTorranceMISSample(misData, float(samples[0]), float(samples[1]));
		if (AiV3Dot(wi, sg->Nf) > 0.0f)
		{
			// get half-angle vector for fresnel
			wi_ray.dir = wi;
			AiV3Normalize(H, wi + wo);
			if (AiTrace(&wi_ray, &scrs))
			{

				AtRGB f = AiCookTorranceMISBRDF(misData, &wi) / AiCookTorranceMISPDF(misData, &wi);
				resultSpecularInDirect += scrs.color * f;
			}
		}
	}
	resultSpecularInDirect *= AiSamplerGetSampleInvCount(sampit);
	resultSpecularInDirect *= color*intensity;

	sg->out.RGB = specResult + resultSpecularInDirect;
}

node_loader
{
	if (i > 0)
		return false;

	node->methods = hnSpecularMtd;
	node->output_type = AI_TYPE_RGB;
	node->name = "specular";
	node->node_type = AI_NODE_SHADER;
	strcpy_s(node->version, AI_VERSION);
	return true;
}
