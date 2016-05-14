#include <ai.h>
#include <string.h>

AI_SHADER_NODE_EXPORT_METHODS(ConstantColorMethods);

enum SimpleParams { p_color };

node_parameters
{
	AiParameterRGB("color", 1.0f, 1.0f, 1.0f);
}

node_initialize {}

node_update {}

node_finish {}

shader_evaluate
{
	sg->out.RGB = AiShaderEvalParamRGB(p_color);

}

node_loader
{
	if (i > 0)
		return false;

	node->methods = ConstantColorMethods;
	node->output_type = AI_TYPE_RGB;
	node->name = "constant";
	node->node_type = AI_NODE_SHADER;
	strcpy_s(node->version, AI_VERSION);
	return true;
}

