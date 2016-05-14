import pymel.core as pm
import mtoa.utils as utils
import mtoa.ui.ae.utils as aeUtils
from mtoa.ui.ae.shaderTemplate import ShaderAETemplate

class AEhnSkinTemplate(ShaderAETemplate):
    def setup(self):
        self.addSwatch()
        self.beginScrollLayout()
       
        self.beginLayout('Diffuse', collapse=False)
        self.addControl('diffuseStrength', label='diffuseStrength')
        self.addControl('diffuseColor', label='diffuseColor')
        self.addControl('diffuseRoughness', label='diffuseRoughness')
        self.endLayout()
		
		self.beginLayout('Specular', collapse=False)
        self.addControl('specularStrength', label='specularStrength')
        self.addControl('specularColor', label='specularColor')
        self.addControl('specularRoughness', label='specularRoughness')
		self.addControl('specularNormal',label='specularNormal')
        self.endLayout()
		
		self.beginLayout('SubSurfaceScattering', collapse=False)
        self.addControl('sssStrength', label='sssStrength')
        self.addControl('sssRadius', label='sssRadius')
        self.addControl('sssRadiusColor', label='sssRadiusColor')
        self.endLayout()

        pm.mel.AEdependNodeTemplate(self.nodeName)

        self.addExtraControls()
        self.endScrollLayout()