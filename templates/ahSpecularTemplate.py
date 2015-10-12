import pymel.core as pm
from alShaders import alShadersTemplate

class AEahSpecularTemplate(alShadersTemplate):
    def setup(self):
        self.addSwatch()
        self.beginScrollLayout()
       
        self.beginLayout('Specular 1', collapse=False)
        self.addControl('specular1Strength', label='Strength')
        self.addControl('specular1Color', label='Color')
        self.addControl('specular1Roughness', label='Roughness')
        self.addControl('specular1Ior', label='IOR')

        self.beginLayout('Advanced')
        self.addControl('specular1RoughnessDepthScale', label='Roughness depth scale')
        self.addControl('specular1ExtraSamples', label='Extra samples')
        self.addControl('specular1Normal', label='Normal')
        self.endLayout() # end Advanced

        self.endLayout() # end Specular 1

        self.beginLayout('Specular 2')
        self.addControl('specular2Strength', label='Strength')
        self.addControl('specular2Color', label='Color')
        self.addControl('specular2Roughness', label='Roughness')
        self.addControl('specular2Ior', label='IOR')

        self.beginLayout('Advanced')
        self.addControl('specular2RoughnessDepthScale', label='Roughness depth scale')
        self.addControl('specular2ExtraSamples', label='Extra samples')
        self.addControl('specular2Normal', label='Normal')
        self.endLayout() # end Advanced
        
        self.endLayout() # end Specular 1

        pm.mel.AEdependNodeTemplate(self.nodeName)

        self.addExtraControls()
        self.endScrollLayout()