import pymel.core as pm
import mtoa.utils as utils
import mtoa.ui.ae.utils as aeUtils
from mtoa.ui.ae.shaderTemplate import ShaderAETemplate

class AEahDiffuseTemplate(ShaderAETemplate):
    def setup(self):
        self.addSwatch()
        self.beginScrollLayout()
       
        self.beginLayout('Diffuse', collapse=False)
        self.addControl('diffuseStrength', label='diffuseStrength')
        self.addControl('diffuseColor', label='diffuseColor')
        self.addControl('diffuseRoughness', label='diffuseRoughness')
        self.endLayout()

        pm.mel.AEdependNodeTemplate(self.nodeName)

        self.addExtraControls()
        self.endScrollLayout()