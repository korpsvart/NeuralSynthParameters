# Neural Synth Parameters

A prototype for a JUCE synthesiser embedding a pre-trained neural network to estimate the synthesiser parameters starting from a target audio file. The used network was trained using the model and code from  [SSSSM-DDSP](https://github.com/korpsvart/SSSSM-DDSP/tree/colab_version) (forked repository containing a version that can be used for training in Google Colab if you don't have an available GPU).

Current limitations:
- No FX units implemented (parameters are present in the GUI but not used)
- Limited polyphony due to heavy computational load
- Still low match quality


<p align="center">
  <img width="800" height="auto" alt="GUI" src="/readme_img/GUINSP.PNG">
</p>