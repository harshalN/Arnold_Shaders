#include <iostream>
#include <ai_sampler.h>
#include <ai.h>

struct ShaderData
{
	AtSampler* diffuse_sampler;
	AtSampler* glossy_sampler;

	int GI_diffuse_depth;
	int GI_specular_depth;
	int GI_diffuse_samples;
	int GI_glossy_samples;

	bool specularNormalConnected;
};

AI_SHADER_NODE_EXPORT_METHODS(hnSkinMtd)

enum hnSkinParams
{
	//diffuse
	p_diffuseStrength = 0,
	p_diffuseColor,
	p_diffuseRoughness,

	//specular
	p_specularStrength,
	p_specularColor,
	p_specularRoughness,
	p_specularNormal,

	//subsurface
	p_sssStrength,
	p_sssRadius,
	p_sssColor

};

node_parameters
{
	//diffuse
	AiParameterFLT("diffuseStrength", 1.0f);
	AiParameterRGB("diffuseColor", 0.18f, 0.18f, 0.18f);
	AiParameterFLT("diffuseRoughness", 0.0f);

	//specular
	AiParameterFLT("specularStrength", 1.0f);
	AiParameterRGB("specularColor", 1.0f, 1.0f, 1.0f);
	AiParameterFLT("specularRoughness", 0.3f);
	AiParameterVec("specularNormal", 0, 0, 0);

	AiParameterFLT("sssStrength", 0.0f);
	AiParameterFLT("sssRadius", 3.6f);
	AiParameterRGB("sssRadiusColor", .439f, .156f, .078f);

}

node_loader
{
	if (i > 0)return 0;
	node->methods		= hnSkinMtd;
	node->output_type	= AI_TYPE_RGB;
	node->name			= "hnSkin";
	node->node_type = AI_NODE_SHADER;
	strcpy_s(node->version, AI_VERSION);
	return true;
}

node_initialize
{
	ShaderData* data = new ShaderData;
	AiNodeSetLocalData(node, data);
	data->diffuse_sampler = NULL;
	data->glossy_sampler = NULL;
};

node_finish
{
	if (AiNodeGetLocalData(node))
	{
		ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

		AiSamplerDestroy(data->diffuse_sampler);
		AiSamplerDestroy(data->glossy_sampler);

		AiNodeSetLocalData(node, NULL);
		delete data;
	}
}

node_update
{
	ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

	AtNode *options = AiUniverseGetOptions();
	data->GI_diffuse_depth = AiNodeGetInt(options, "GI_diffuse_depth");
	data->GI_diffuse_samples = AiNodeGetInt(options, "GI_diffuse_samples");
	data->diffuse_sampler = AiSampler(data->GI_diffuse_samples, 2);
	
	data->GI_specular_depth = AiNodeGetInt(options, "GI_glossy_depth");
	data->GI_glossy_samples = AiNodeGetInt(options, "GI_glossy_samples");
	data->glossy_sampler = AiSampler(data->GI_glossy_samples, 2);

	data->specularNormalConnected = AiNodeIsLinked(node, "specularNormal");

	AiAOVRegister("diffuse", AI_TYPE_RGB, AI_AOV_BLEND_OPACITY);
	AiAOVRegister("specular", AI_TYPE_RGB, AI_AOV_BLEND_OPACITY);
	AiAOVRegister("subsurface", AI_TYPE_RGB, AI_AOV_BLEND_OPACITY);

}

shader_evaluate
{
	ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

	float roughness = AiShaderEvalParamFlt(p_specularRoughness);
	float diffuseRoughness = AiShaderEvalParamFlt(p_diffuseRoughness);
	float diffuseStrength = AiShaderEvalParamFlt(p_diffuseStrength);
	float specularStrength = AiShaderEvalParamFlt(p_specularStrength);

	AtRGB specularColor = AiShaderEvalParamRGB(p_specularColor);
	AtRGB diffuseColor = AiShaderEvalParamRGB(p_diffuseColor);

	
	//evaluate specular normal connection
	AtVector specularNormal = sg->Nf;
	if (data->specularNormalConnected)
		specularNormal = AiShaderEvalParamVec(p_specularNormal);
	
	// build a local frame for sampling
	AtVector U, V;
	AiBuildLocalFramePolar(&U, &V, &sg->N);

	//direct lighting
	//initialise diffuse
	AtRGB resultDiffuseDirect = AI_RGB_BLACK;
	void* diffuseMISData = AiOrenNayarMISCreateData(sg, diffuseRoughness);
	
	//initialise specular
	AtRGB resultSpecularDirect = AI_RGB_BLACK;
	//swap specular normal
	AtVector Nprev = sg->Nf;
	sg->Nf = specularNormal;
	void* specularMISData = AiCookTorranceMISCreateData(sg, &U, &V, roughness, roughness);
	//swap back original normal
	sg->Nf = Nprev;


	AiLightsPrepare(sg);
	while (AiLightsGetSample(sg))
	{
		AtRGB lightColor = AiLightGetColor(sg->Lp);
		float light_diffuseStrength  = AiLightGetDiffuse(sg->Lp);
		float light_specularStrength = AiLightGetSpecular(sg->Lp);

		resultDiffuseDirect += AiEvaluateLightSample(sg, diffuseMISData, AiOrenNayarMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF)
			*light_diffuseStrength;
		resultSpecularDirect += AiEvaluateLightSample(sg, specularMISData, AiCookTorranceMISSample, AiCookTorranceMISBRDF, AiCookTorranceMISPDF)
			*light_specularStrength;
	}

	resultDiffuseDirect *= diffuseColor*diffuseStrength;
	resultSpecularDirect *= specularColor*specularStrength;

	//indirect lighting
	//initialise specular
	AtRGB resultSpecularInDirect = AI_RGB_BLACK;
	//initialise diffuse
	AtRGB resultDiffuseInDirect = AI_RGB_BLACK;

	float samples[2];
	AtRay wi_ray;
	AtVector wi;
	AtScrSample scrs;
	AtVector H;

	//Camera ray direction in shading coordinates, omega_o
	AtVector wo = -sg->Rd;

	//indirect specular
	AtSamplerIterator* sampit = AiSamplerIterator(data->glossy_sampler, sg);
	AiMakeRay(&wi_ray, AI_RAY_GLOSSY, &sg->P, NULL, AI_BIG, sg);
	sg->Nf = specularNormal;
	while (AiSamplerGetSample(sampit, samples))
	{
		wi = AiCookTorranceMISSample(specularMISData, float(samples[0]), float(samples[1]));
		if (AiV3Dot(wi, specularNormal) > 0.0f)
		{
			// get half-angle vector for fresnel
			wi_ray.dir = wi;
			AiV3Normalize(H, wi + wo);
			if (AiTrace(&wi_ray, &scrs))
			{

				AtRGB f = AiCookTorranceMISBRDF(specularMISData, &wi) / AiCookTorranceMISPDF(specularMISData, &wi);
				resultSpecularInDirect += scrs.color * f;
			}
		}
	}
	sg->Nf = Nprev;
	resultSpecularInDirect *= AiSamplerGetSampleInvCount(sampit);
	resultSpecularInDirect *= specularColor*specularStrength;

	//indirect diffuse
	sampit = NULL;
	sampit = AiSamplerIterator(data->diffuse_sampler, sg);
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
			AtRGB f = AiOrenNayarMISBRDF(diffuseMISData, &wi) / p;
			resultDiffuseInDirect += scrs.color * f;
		}
	}
		resultDiffuseInDirect *= AiSamplerGetSampleInvCount(sampit);
		resultDiffuseInDirect *= diffuseColor*diffuseStrength;
	
	//Sub-surface scattering
	float sssStrength = AiShaderEvalParamFlt(p_sssStrength);
	float sssRadius = AiShaderEvalParamFlt(p_sssRadius);
	AtRGB sssColor = AiShaderEvalParamRGB(p_sssColor);
	AtRGB resultSSS = AiBSSRDFCubic(sg, &sssRadius, &sssColor) * sssStrength;

	AiAOVSetRGB(sg, "diffuse", resultDiffuseDirect + resultDiffuseInDirect);
	AiAOVSetRGB(sg, "specular", resultSpecularDirect + resultSpecularInDirect);
	AiAOVSetRGB(sg, "subsurface", resultSSS);

	sg->out.RGB = resultDiffuseDirect
		+ resultDiffuseInDirect
		+ resultSpecularDirect
		+ resultSpecularInDirect
		+ resultSSS;
}


