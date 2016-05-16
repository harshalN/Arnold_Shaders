/*Artist friendly hair shader with efficient importance sampling
This shader uses sadeghi et al hair shading model, which is a simplified
 version of the physically based marschner model. And uses the importance 
sampling methods presented in ISHair paper. Single-scattering only.
The attribute naming isn't artist friendly yet, but the paper title refers
to the individual and intuitive controls of the shading system*/

#include<ai.h>
#include <ai_sampler.h>
#include<string.h>
#include<math.h>
#include<ai_constants.h>

#define PI AI_PI

struct HairShadingData
{
	float phi_R, theta_R;//angles of eye vector in shading coordinates as shown in Marschner's paper
	float alpha_R, beta_R, alpha_TT, beta_TT,
		alpha_TRT, beta_TRT, gamma_TT, gamma_g;
	float alpha_X, beta_X;
	//derived quantities
	float A_R, A_TT, A_TRT,A_X,B_X;
	float B_R, B_TT, B_TRT;
	float C_TT, C_g, D_g;
	float I_R, I_TT, I_TRT, I_g;
	float E_R, E_TT, E_TRT, E_g;
};

AtColor hnHairMISBRDF(const void* brdf_data, const AtVector* indir)
{
	const  HairShadingData* data = (HairShadingData) brdf_data;
	float f;
	f = data->I_R*M_R(data->theta_j)*N_R(data->phi) / (cos(data->theta_d)) ^ 2
		+ data->I_TT*M_TT(data->theta_h)*N_TT(data->phi) / (cos(data->theta_d)) ^ 2
		+ data->I_TRT*M_TRT(data->theta_h)*N_TRT - g(data->phi) / (cos(data->theta_d)) ^ 2
		+ ddata->I_TRT*M_TRT(data->theta_h)*data->I_g*N_g(data->phi) / cos(data->theta_d) ^ 2;
	
	return AtColor(f,f,f);
}

float hnHairMISPDF(const void* brdf_data, const AtVector* indir)
{
	/*TODO: How to distribute samples within lobes proportional to intensities
	while still maintaining Arnold API function prototypes. This would be
	easy to do if I write this shader as a shader and not as a BRDF conforming to
	the prototype definitions of Arnold API.
	Currently I'm just sampling only one lobe with max energy.
	Energies are created and compared in CreateData method*/
	float p_theta_i = (1 / 2 * cos(theta_i)(data->A_x - data->Bx))*(data->beta_X)
		/ ((theta_h - data->alpha_X) ^ 2 + data->beta_X);

	/*TODO: select appropriate sampling function for azimuthal terms
	based on energies*/
	/*TODO - generate random variable eta*/
	float eta = 0.0f;

	//sampling N_R
	float phi = 2 * asin(2 * rx - 1); //rx is a random variable

	
	//sampling
	
}

AtVector hnHairMISSample(const void* brdf_data, float rx, float ry)
{
	HairShadingData* data = (HairShadingData*)brdf_data;
	float theta_i = 2 * data->beta_X* tan(rx*(data->A_X - data->B_X)+
		data->B_X) + 2 * data->alpha_X - data->theta_R;
	float phi = 2 * asin(2 * rx - 1);

	AtVector L_i = AI_V3_ZERO;
	
	//TODO: construct incident vector from given angles
	return L_i;
}

/*The CreateData function does not have a typedef prototype in Arnold API*/
void* hnHairMISCreateData(const AtShaderGlobals* sg, const AtVector* U, const AtVector* V,
	float alpha_R,float beta_R,float alpha_TT,float beta_TT, 
	float alpha_TRT, float beta_TRT,float gamma_TT, float gamma_g,
	float I_R,float I_TT, float I_TRT, float I_g)
{
	HairShadingData* data = new HairShadingData;
	data->E_R = 4 * sqrt(2 * PI)*beta_R*I_R;
	data->E_TT = 2 * PI*beta_TT*gamma_TT*I_TT;
	data->E_TRT = 4 * sqrt(2 * PI)*beta_TRT*I_TRT;
	data->E_g = 4 * PI*beta_TRT*gamma_g*I_TRT*I_g;

	if ((data->E_R > data->E_TT) && (data->E_R > data->E_TRT) && (data->E_R > data->E_g))
	{
		data->alpha_X = data->alpha_R;
		data->beta_X = data->beta_R;
	}
	else if ((data->E_TT > data->E_R) && (data->E_TT > data->E_TRT) && (data->E_TT > data->E_g))
	{
		data->alpha_X = data->alpha_TT;
		data->beta_X = data->beta_TT;
	}
	else
	{
		data->alpha_X = data->alpha_TRT;
		data->beta_X = data->beta_TRT;
	}
	
}

AI_SHADER_NODE_EXPORT_METHODS(hnHairMtd);


enum DiffuseParams {
	p_color,
	p_alpha_R,
	p_beta_R,
	p_alpha_TT,
	p_beta_TT,
	p_alpha_TRT,
	p_beta_TRT
};


AI_SHADER_NODE_EXPORT_METHODS(hnHairMtd);

enum DiffuseParams {
	p_color,
	p_alpha_R,
	p_beta_R,
	p_alpha_TT,
	p_beta_TT,
	p_alpha_TRT,
	p_beta_TRT,
	p_gamma_TT,
	p_gamma_g,
	p_I_R,
	p_I_TT,
	p_I_TRT,
	p_I_g
};

node_parameters
{
	AiParameterRGB("color", 1.0f, 1.0f, 1.0f);
	AiParameterFlt("alpha_R", 0.0f);
	AiParameterFlt("beta_R", 0.0f);
	AiParameterFlt("alpha_TT", 0.0f);
	AiParameterFlt("beta_TT", 0.0f);
	AiParameterFlt("alpha_TRT", 0.0f);
	AiParameterFlt("beta_TRT", 0.0f);
	AiParameterFlt("gamma_TT", 0.0f);
	AiParameterFlt("gamma_g", 0.0f);
	AiParameterFlt("I_R", 0.0f);
	AiParameterFlt("I_TT", 0.0f);
	AiParameterFlt("I_TRT", 0.0f);
	AiParameterFlt("I_g", 0.0f);
}

node_initialize
{}

node_update
{}

node_finish
{}

shader_evaluate
{
	AtColor color = AiShaderEvalParamRGB(p_color);
	float alpha_R = AiShaderEvalParamFlt(p_alpha_R);
	float beta_R = AiShaderEvalParamFlt(p_beta_R);
	float alpha_TT = AiShaderEvalParamFlt(p_alpha_TT);
	float beta_TT = AiShaderEvalParamFlt(p_beta_TT);
	float alpha_TRT = AiShaderEvalParamFlt(p_alpha_TRT);
	float beta_TRT = AiShaderEvalParamFlt(p_beta_TRT);
	float gamma_TT = AiShaderEvalParamFlt(p_gamma_TT);
	float gamma_g = AiShaderEvalParamFlt(p_gamma_g);
	float I_R = AiShaderEvalParamFlt(p_I_R);
	float I_TT = AiShaderEvalParamFlt(p_I_TT);
	float I_TRT = AiShaderEvalParamFlt(p_I_TRT);
	float I_g = AiShaderEvalParamFlt(p_I_g);

	void* hairData = hnHairMISCreateData(sg, U, V, alpha_R, beta_R,
		alpha_TT, beta_TT, alpha_TRT, beta_TRT, gamma_TT, gamma_g,
		I_R, I_TT, I_TRT, I_g);

	AiLightsPrepare(sg);
	AtColor hairSample = AI_RGB_BLACK;
	AtColor hairResult = AI_RGB_BLACK;
	float lgt_int;
	while (AiLightsGetSample(sg))
	{
		lgt_int = AiLightGetIntensity(sg->Lp);
		//Call AiEvaluateLightSample with my brdf functions
		hairSample = AiEvaluateLightSample(sg, hairData, hnHairMISSample, hnHairMISBRDF, hnHairMISPDF);
		hairResult += hairSample; //unnecessary extra var, but code stays cleaner
	}

	/*TODO - indirect lighting
	Now that I have implemented brdfs in Arnold API prototype formats
	I must be able to call built in integration functions rather than 
	writing my code from scratch for integrating indirect lighting.
	Must investigate this further.*/

}