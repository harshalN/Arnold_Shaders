#include <iostream>
#include <ai_sampler.h>
#include <ai.h>
#include <ImathVec.h>
#include <ImathMatrix.h>
#include <ImathMatrixAlgo.h>
#include <map>

#include "alUtil.h"
#include "MIS.h"
#include "alSurface.h"

//Gives the renderer a pointer to the shader's methods
AI_SHADER_NODE_EXPORT_METHODS(ahSpecularMtd)


#define GlossyMISBRDF AiCookTorranceMISBRDF	//Returns the Cook-Torrance microfacet BRDF 
//multiplied by the cosine of the angle between the incoming light direction and the surface normal. 
//This function is useable by AiEvaluateLightSample() to perform MIS of the built - in Cook - Torrance BRDF

#define GlossyMISPDF AiCookTorranceMISPDF  //Returns the result of the PDF used to importance sample 
//the Cook-Torrance BRDF for the given direction. This function is useable by AiEvaluateLightSample() 
//to perform MIS of the built - in Cook - Torrance BRDF

#define GlossyMISSample AiCookTorranceMISSample  //Returns a sample direction for the Cook-Torrance BRDF given two random values. 
//The samples are generated using importance sampling and have a probability density defined by AiCookTorranceMISPDF().
//This function is useable by AiEvaluateLightSample() to perform MIS of the built - in Cook - Torrance BRDF.


#define GlossyMISBRDF_wrap AiCookTorranceMISBRDF_wrap
#define GlossyMISPDF_wrap AiCookTorranceMISPDF_wrap
#define GlossyMISSample_wrap AiCookTorranceMISSample_wrap

#define GlossyMISCreateData AiCookTorranceMISCreateData  //Returns a pointer to the data required by AiEvaluateLightSample() 
//to sample the Cook-Torrance BRDF using multiple importance sampling. 
//The returned data is allocated in a pixel pool and does not need to be freed by the caller.



#define NUM_LIGHT_GROUPS 8

#define NUM_ID_AOVS 8

typedef float AtFloat;

inline void flipNormals(AtShaderGlobals* sg)
{
	sg->Nf = -sg->Nf;
	sg->Ngf = -sg->Ngf;
}

enum ahSpecularParams
{
	// specular
	p_specular1Strength,
	p_specular1Color,
	p_specular1Roughness,
	p_specular1Ior,
	p_specular1RoughnessDepthScale,
	p_specular1ExtraSamples,
	p_specular1Normal,
	p_specular2Strength,
	p_specular2Color,
	p_specular2Roughness,
	p_specular2Ior,
	p_specular2RoughnessDepthScale,
	p_specular2ExtraSamples,
	p_specular2Normal,

};

node_parameters
{
	AiParameterFLT("specular1Strength", 1.0f);
	AiParameterRGB("specular1Color", 1.0f, 1.0f, 1.0f);
	AiParameterFLT("specular1Roughness", 0.3f);
	AiParameterFLT("specular1Ior", 1.4f);
	AiParameterFLT("specular1RoughnessDepthScale", 1.5f);
	AiParameterINT("specular1ExtraSamples", 0);
	AiParameterVec("specular1Normal", 0, 0, 0);

	AiParameterFLT("specular2Strength", 0.0f);
	AiParameterRGB("specular2Color", 1.0f, 1.0f, 1.0f);
	AiParameterFLT("specular2Roughness", 0.3f);
	AiParameterFLT("specular2Ior", 1.4f);
	AiParameterFLT("specular2RoughnessDepthScale", 1.5f);
	AiParameterINT("specular2ExtraSamples", 0);
	AiParameterVec("specular2Normal", 0, 0, 0);
}

#ifdef MSVC
#define _CRT_SECURE_NO_WARNINGS 1
#endif
node_loader
{
	if (i>0) return 0;
	node->methods = ahSpecularMtd;
	node->output_type = AI_TYPE_RGB;
	node->name = "ahSpecular";
	node->node_type = AI_NODE_SHADER;
	strcpy_s(node->version, AI_VERSION);
	return true;
}

node_initialize
{
	ShaderData* data = new ShaderData;
	AiNodeSetLocalData(node, data); //Store custom data in the node
	data->diffuse_sampler = NULL;
	data->glossy_sampler = NULL;
	data->glossy2_sampler = NULL;
	data->refraction_sampler = NULL;
	data->backlight_sampler = NULL;
};

node_finish
{
	if (AiNodeGetLocalData(node))
	{
		ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);

		AiSamplerDestroy(data->diffuse_sampler);
		AiSamplerDestroy(data->glossy_sampler);
		AiSamplerDestroy(data->glossy2_sampler);
		AiSamplerDestroy(data->refraction_sampler);
		AiSamplerDestroy(data->backlight_sampler);

		AiNodeSetLocalData(node, NULL);
		delete data;
	}
}


node_update
{
	ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);

	
	

	// store some options we'll reuse later
	AtNode *options = AiUniverseGetOptions();
	data->GI_diffuse_depth = AiNodeGetInt(options, "GI_diffuse_depth");
	data->GI_reflection_depth = AiNodeGetInt(options, "GI_reflection_depth");
	data->GI_refraction_depth = AiNodeGetInt(options, "GI_refraction_depth");
	data->GI_glossy_depth = AiNodeGetInt(options, "GI_glossy_depth");
	data->GI_glossy_samples = AiNodeGetInt(options, "GI_glossy_samples") + params[p_specular1ExtraSamples].INT;
	data->glossy_samples2 = SQR(data->GI_glossy_samples);
	data->GI_glossy2_samples = AiNodeGetInt(options, "GI_glossy_samples") + params[p_specular2ExtraSamples].INT;
	data->glossy2_samples2 = SQR(data->GI_glossy2_samples);
	data->refraction_samples2 = SQR(data->GI_refraction_samples);

	// setup samples
	AiSamplerDestroy(data->glossy_sampler);
	AiSamplerDestroy(data->glossy2_sampler);
	data->glossy_sampler = AiSampler(data->GI_glossy_samples, 2);
	data->glossy2_sampler = AiSampler(data->GI_glossy_samples, 2);
	
	// check whether the normal parameters are connected or not
	data->specular1NormalConnected = AiNodeIsLinked(node, "specular1Normal");
	data->specular2NormalConnected = AiNodeIsLinked(node, "specular2Normal");
}


shader_evaluate
{
	ShaderData *data = (ShaderData*)AiNodeGetLocalData(node);

	float ior = std::max(1.001f, AiShaderEvalParamFlt(p_specular1Ior));
	float roughness = AiShaderEvalParamFlt(p_specular1Roughness);
	roughness *= roughness;
	
	// Initialize parameter temporaries
	// TODO: reorganize this so we're not evaluating upstream when we don't need the parameters, e.g. in shadow rays
	AtRGB specular1Color = AiShaderEvalParamRGB(p_specular1Color) * AiShaderEvalParamFlt(p_specular1Strength);
	AtRGB specular2Color = AiShaderEvalParamRGB(p_specular2Color) * AiShaderEvalParamFlt(p_specular2Strength);
	AtVector specular1Normal = sg->Nf;
	if (data->specular1NormalConnected)
	{
		specular1Normal = AiShaderEvalParamVec(p_specular1Normal);
	}

	AtVector specular2Normal = sg->Nf;
	if (data->specular2NormalConnected)
	{
		specular2Normal = AiShaderEvalParamVec(p_specular2Normal);
	}

	float roughness2 = AiShaderEvalParamFlt(p_specular2Roughness);
	roughness2 *= roughness2;

	float eta = 1.0f / ior;
	float ior2 = std::max(1.001f, AiShaderEvalParamFlt(p_specular2Ior));
	float eta2 = 1.0f / ior2;


	float specular1RoughnessDepthScale = AiShaderEvalParamFlt(p_specular1RoughnessDepthScale);
	float specular2RoughnessDepthScale = AiShaderEvalParamFlt(p_specular2RoughnessDepthScale);
	
	// Grab the roughness from the previous surface and make sure we're slightly rougher than it to avoid glossy-glossy fireflies
	float alsPreviousRoughness = 0.05f;
	AiStateGetMsgFlt("alsPreviousRoughness", &alsPreviousRoughness);
	if (sg->Rr > 0)
	{
		roughness = std::max(roughness, alsPreviousRoughness*specular1RoughnessDepthScale);
		roughness2 = std::max(roughness2, alsPreviousRoughness*specular2RoughnessDepthScale);
		//transmissionRoughness = std::max(transmissionRoughness, alsPreviousRoughness*transmissionRoughnessDepthScale);
	}

	// clamp roughnesses
	roughness = std::max(0.000001f, roughness);
	roughness2 = std::max(0.000001f, roughness2);

	// Initialize result temporaries
	AtRGB result_glossyDirect = AI_RGB_BLACK;
	AtRGB result_glossy2Direct = AI_RGB_BLACK;
	AtRGB result_glossyIndirect = AI_RGB_BLACK;
	AtRGB result_glossy2Indirect = AI_RGB_BLACK;

	// Set up flags to early out of calculations based on where we are in the ray tree
	bool do_glossy = true;
	bool do_glossy2 = true;
	int glossy_samples = data->GI_glossy_samples;
	int diffuse_samples = data->GI_diffuse_samples;


	// build a local frame for sampling
	AtVector U, V;
	AiBuildLocalFramePolar(&U, &V, &sg->N);

	// View direction, omega_o
	AtVector wo = -sg->Rd;

	// Accumulator for transmission integrated according to the specular1 brdf. Will be used to attenuate diffuse,
	// glossy2, sss and transmission
	float kti = 1.0f;
	float kti2 = 1.0f;

	// Begin illumination calculation

	// Create the BRDF data structures for MIS
	// {
	AtVector Nold = sg->N;
	AtVector Nfold = sg->Nf;
	sg->N = sg->Nf = specular1Normal;
	void* mis;
	mis = GlossyMISCreateData(sg, &U, &V, roughness, roughness);
	BrdfData_wrap brdfw;
	brdfw.brdf_data = mis;
	brdfw.sg = sg;
	brdfw.eta = eta;
	brdfw.V = wo;
	brdfw.N = specular1Normal;
	brdfw.kr = 0.0f;


	sg->N = sg->Nf = specular2Normal;
	void* mis2;
	mis2 = GlossyMISCreateData(sg, &U, &V, roughness2, roughness2);
	BrdfData_wrap brdfw2;
	brdfw2.brdf_data = mis2;
	brdfw2.sg = sg;
	brdfw2.eta = eta2;
	brdfw2.V = wo;
	brdfw2.N = specular2Normal;
	brdfw2.kr = 0.0f;

	sg->N = Nold;

	// }

	// Light loop
	AiLightsPrepare(sg);
	if (sg->Rt & AI_RAY_CAMERA)
	{
		AtRGB LspecularDirect, Lspecular2Direct;
		while (AiLightsGetSample(sg))
		{
			// per-light specular and diffuse strength multipliers
			float specular_strength = AiLightGetSpecular(sg->Lp);
			if (do_glossy)
			{
				// override the specular normal
				sg->Nf = specular1Normal;
				// evaluate this light sample
				LspecularDirect =
					AiEvaluateLightSample(sg, &brdfw.brdf_data, AiCookTorranceMISSample, GlossyMISBRDF_wrap, GlossyMISPDF_wrap)
					* specular_strength;
				// accumulate the result
				result_glossyDirect += LspecularDirect;
				// put back the original surface normal
				sg->Nf = Nfold;
			}
			if (do_glossy2)
			{
				sg->Nf = specular2Normal;
				AtFloat r = (1.0f - brdfw.kr*maxh(specular1Color));
				Lspecular2Direct =
					AiEvaluateLightSample(sg, &brdfw2, GlossyMISSample_wrap, GlossyMISBRDF_wrap, GlossyMISPDF_wrap)
					* r * specular_strength;
				result_glossy2Direct += Lspecular2Direct;
				sg->Nf = Nfold;
			}
		}
	}
	else
	{
		while (AiLightsGetSample(sg))
		{
			float specular_strength = AiLightGetSpecular(sg->Lp);
			float diffuse_strength = AiLightGetDiffuse(sg->Lp);
			if (do_glossy)
			{
				result_glossyDirect +=
					AiEvaluateLightSample(sg, &brdfw, GlossyMISSample_wrap, GlossyMISBRDF_wrap, GlossyMISPDF_wrap)
					* specular_strength;
			}
			if (do_glossy2)
			{
				result_glossy2Direct +=
					AiEvaluateLightSample(sg, &brdfw2, GlossyMISSample_wrap, GlossyMISBRDF_wrap, GlossyMISPDF_wrap)
					* (1.0f - brdfw.kr*maxh(specular1Color))
					* specular_strength;
			}
		}
	}

	sg->fhemi = true;

	// Multiply by the colors
	result_glossyDirect *= specular1Color;
	result_glossy2Direct *= specular2Color;

	// Sample BRDFS
	float samples[2];
	AtRay wi_ray;
	AtVector wi;
	AtScrSample scrs;
	AtVector H;
	float kr = 1, kt = 1;

	// indirect_specular
	// -----------------
	if (do_glossy)
	{
		AtSamplerIterator* sampit = AiSamplerIterator(data->glossy_sampler, sg);
		AiMakeRay(&wi_ray, AI_RAY_GLOSSY, &sg->P, NULL, AI_BIG, sg);
		kti = 0.0f;
		AiStateSetMsgFlt("alsPreviousRoughness", roughness);
		sg->Nf = specular1Normal;
		while (AiSamplerGetSample(sampit, samples))
		{
			wi = GlossyMISSample(mis, float(samples[0]), float(samples[1]));
			if (AiV3Dot(wi, specular1Normal) > 0.0f)
			{
				// get half-angle vector for fresnel
				wi_ray.dir = wi;
				AiV3Normalize(H, wi + brdfw.V);
				kr = fresnel(std::max(0.0f, AiV3Dot(H, wi)), eta);
				kti += kr;
				if (kr > IMPORTANCE_EPS) // only trace a ray if it's going to matter
				{
					// if we're in a camera ray, pass the sample index down to the child SG
					if (AiTrace(&wi_ray, &scrs))
					{
						AtRGB f = GlossyMISBRDF(mis, &wi) / GlossyMISPDF(mis, &wi) * kr;
						result_glossyIndirect += scrs.color * f;

					}
				}
			}
		}
		sg->Nf = Nfold;
		result_glossyIndirect *= AiSamplerGetSampleInvCount(sampit);
		kti *= AiSamplerGetSampleInvCount(sampit);
		kti = 1.0f - kti*maxh(specular1Color);
		result_glossyIndirect *= specular1Color;

		
	} // if (do_glossy)

	// indirect_specular2
	// ------------------
	if (do_glossy2)
	{
		AtSamplerIterator* sampit = AiSamplerIterator(data->glossy2_sampler, sg);
		AiMakeRay(&wi_ray, AI_RAY_GLOSSY, &sg->P, NULL, AI_BIG, sg);
		kti2 = 0.0f;
		AiStateSetMsgFlt("alsPreviousRoughness", roughness2);
		sg->Nf = specular2Normal;
		while (AiSamplerGetSample(sampit, samples))
		{
			wi = GlossyMISSample(mis2, float(samples[0]), float(samples[1]));
			if (AiV3Dot(wi, specular2Normal) > 0.0f)
			{
				wi_ray.dir = wi;
				AiV3Normalize(H, wi + brdfw2.V);
				// add the fresnel for this layer
				kr = fresnel(std::max(0.0f, AiV3Dot(H, wi)), eta2);
				if (kr > IMPORTANCE_EPS) // only trace a ray if it's going to matter
				{
					if (AiTrace(&wi_ray, &scrs))
					{
						AtRGB f = GlossyMISBRDF(mis2, &wi) / GlossyMISPDF(mis2, &wi) * kr * kti;
						result_glossy2Indirect += scrs.color*f;
						kti2 += kr;

					}
				}
			}
		}
		sg->Nf = Nfold;
		result_glossy2Indirect *= AiSamplerGetSampleInvCount(sampit);
		kti2 *= AiSamplerGetSampleInvCount(sampit);
		kti2 = 1.0f - kti2*maxh(specular2Color);
		result_glossy2Indirect *= specular2Color;
	} // if (do_glossy)



	// Sum final result from temporaries
	//
	sg->out.RGB = result_glossyDirect + result_glossy2Direct;
}
