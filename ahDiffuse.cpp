#include<ai.h>
#include<iostream>

AI_SHADER_NODE_EXPORT_METHODS(ahDiffuseSurfaceMtd)

enum ahDiffuseParams
{
	id_factor,
	id_color,
	id_roughness
};

node_parameters
{
	AiParameterFLT("diffuse_factor", 1.0f);
	AiParameterRGB("diffuseColor", 1.0f, 1.0f, 1.0f);
	AiParameterFLT("roughness", 0.0f);
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

};

node_finish
{};

node_update
{};

shader_evaluate
{
	float diffuseFactor = AiShaderEvalParamFlt(id_factor);
	float roughness = AiShaderEvalParamFlt(id_roughness);
	AtRGB diffuseColor = AiShaderEvalParamRGB(id_color);
	AtRGB directDiffuse, indirectDiffuse = AI_RGB_BLACK;

	void* diffuseMISData = AiOrenNayarMISCreateData(sg, roughness);
	//direct diffuse
	AiLightsPrepare(sg);
	if (sg->Rt & AI_RAY_CAMERA)
	{
		while (AiLightsGetSample(sg))
		{
			directDiffuse = AiEvaluateLightSample(sg, diffuseMISData, AiOrenNayarMISSample, AiOrenNayarMISBRDF, AiOrenNayarMISPDF);
		}
		directDiffuse *= diffuseFactor*diffuseColor;
	}

	//TODO indirect diffuse

	AtRGB resultDiffuse = directDiffuse + indirectDiffuse;
	sg->out.RGB = resultDiffuse;
}