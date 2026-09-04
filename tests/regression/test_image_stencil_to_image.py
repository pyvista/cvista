"""Regression coverage for the vtkImageStencilToImage core-tier wrapper."""


def test_image_stencil_to_image_imports_and_converts():
    from cvista import vtkImageStencilToImage as flat_class
    from cvista.vtkImagingCore import vtkImageStencilData
    from cvista.vtkImagingStencil import vtkImageStencilToImage

    assert flat_class is vtkImageStencilToImage

    stencil = vtkImageStencilData()
    stencil.SetExtent(0, 2, 0, 1, 0, 0)
    stencil.SetSpacing(1.0, 1.0, 1.0)
    stencil.SetOrigin(0.0, 0.0, 0.0)
    stencil.AllocateExtents()
    stencil.InsertNextExtent(1, 1, 0, 0)

    converter = vtkImageStencilToImage()
    converter.SetInputData(stencil)
    converter.SetInsideValue(7)
    converter.SetOutsideValue(2)
    converter.Update()

    output = converter.GetOutput()
    assert output.GetExtent() == (0, 2, 0, 1, 0, 0)
    assert output.GetScalarTypeAsString() == "unsigned char"
    assert [
        [output.GetScalarComponentAsDouble(x, y, 0, 0) for x in range(3)]
        for y in range(2)
    ] == [[2.0, 7.0, 2.0], [2.0, 2.0, 2.0]]
