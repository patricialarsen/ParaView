/*=========================================================================

  Program:   ParaView
  Module:    vtkHyperTreeGridRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkHyperTreeGridRepresentation.h"

#include "vtkActor.h"
#include "vtkAlgorithmOutput.h"
#include "vtkDataSet.h"
#include "vtkHyperTreeGrid.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkMultiProcessController.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLHyperTreeGridMapper.h"
#include "vtkPVGeometryFilter.h"
#include "vtkPVRenderView.h"
#include "vtkPVTrivialProducer.h"
#include "vtkProcessModule.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkScalarsToColors.h"
#include "vtkSelection.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTexture.h"
#include "vtkTransform.h"

#include <vtk_jsoncpp.h>
#include <vtksys/SystemTools.hxx>

//*****************************************************************************

vtkStandardNewMacro(vtkHyperTreeGridRepresentation);
//----------------------------------------------------------------------------

vtkHyperTreeGridRepresentation::vtkHyperTreeGridRepresentation()
{
  vtkMath::UninitializeBounds(this->VisibleDataBounds);
  this->SetupDefaults();
}

//----------------------------------------------------------------------------
vtkHyperTreeGridRepresentation::~vtkHyperTreeGridRepresentation() = default;

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetupDefaults()
{
  vtkNew<vtkSelection> sel;
  this->Mapper->SetSelection(sel);

  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetProperty(this->Property);

  // Not insanely thrilled about this API on vtkProp about properties, but oh
  // well. We have to live with it.
  vtkNew<vtkInformation> keys;
  this->Actor->SetPropertyKeys(keys);
}

//----------------------------------------------------------------------------
int vtkHyperTreeGridRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkHyperTreeGrid");

  // Saying INPUT_IS_OPTIONAL() is essential, since representations don't have
  // any inputs on client-side (in client-server, client-render-server mode) and
  // render-server-side (in client-render-server mode).
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);

  return 1;
}

//----------------------------------------------------------------------------
int vtkHyperTreeGridRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
  {
    // i.e. this->GetVisibility() == false, hence nothing to do.
    return 0;
  }

  if (request_type == vtkPVView::REQUEST_UPDATE())
  {
    // provide the "geometry" to the view so the view can delivery it to the
    // rendering nodes as and when needed.
    vtkPVView::SetPiece(inInfo, this, this->GetInput(0));

    // We want to let vtkPVRenderView do redistribution of data as necessary,
    // and use this representations data for determining a load balanced distribution
    // if ordered is needed.
    vtkPVRenderView::SetOrderedCompositingConfiguration(inInfo, this,
      vtkPVRenderView::DATA_IS_REDISTRIBUTABLE | vtkPVRenderView::USE_DATA_FOR_LOAD_BALANCING);

    outInfo->Set(
      vtkPVRenderView::NEED_ORDERED_COMPOSITING(), this->NeedsOrderedCompositing() ? 1 : 0);
  }
  else if (request_type == vtkPVView::REQUEST_RENDER())
  {
    this->SetVisibility(true);
    auto data = vtkPVView::GetDeliveredPiece(inInfo, this);
    this->Mapper->SetInputDataObject(data);

    // This is called just before the vtk-level render. In this pass, we simply
    // pick the correct rendering mode and rendering parameters.
    // (when interactive LOD in PV for example)
    this->UpdateColoringParameters();
  }

  return 1;
}

//----------------------------------------------------------------------------
int vtkHyperTreeGridRepresentation::RequestUpdateExtent(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);

  return 1;
}

//----------------------------------------------------------------------------
int vtkHyperTreeGridRepresentation::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
  {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    if (inInfo->Has(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()))
    {
      vtkAlgorithmOutput* aout = this->GetInternalOutputPort();
      vtkPVTrivialProducer* prod = vtkPVTrivialProducer::SafeDownCast(aout->GetProducer());
      if (prod)
      {
        prod->SetWholeExtent(inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()));
      }
    }
    this->Mapper->SetInputConnection(this->GetInternalOutputPort());
  }

  // essential to re-execute geometry filter consistently on all ranks since it
  // does use parallel communication (see #19963).
  this->Mapper->Modified();
  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
vtkDataObject* vtkHyperTreeGridRepresentation::GetRenderedDataObject(int vtkNotUsed(port))
{
  return this->Mapper->GetInput();
}

//----------------------------------------------------------------------------
bool vtkHyperTreeGridRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->AddActor(this->Actor);
    // The HTG Mapper requires parallel projection if adaptive decimation is on
    rview->SetParallelProjection(true);

    // Indicate that this is prop that we are rendering when hardware selection
    // is enabled.
    rview->RegisterPropForHardwareSelection(this, this->GetRenderedProp());
    return this->Superclass::AddToView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkHyperTreeGridRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->RemoveActor(this->Actor);
    rview->UnRegisterPropForHardwareSelection(this, this->GetRenderedProp());
    return this->Superclass::RemoveFromView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetRepresentation(const char* type)
{
  if (vtksys::SystemTools::Strucmp(type, "Wireframe") == 0)
  {
    this->SetRepresentation(WIREFRAME);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Surface") == 0)
  {
    this->SetRepresentation(SURFACE);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Surface With Edges") == 0)
  {
    this->SetRepresentation(SURFACE_WITH_EDGES);
  }
  else
  {
    vtkErrorMacro("Invalid type: " << type);
  }
}

//----------------------------------------------------------------------------
const char* vtkHyperTreeGridRepresentation::GetColorArrayName()
{
  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    return info->Get(vtkDataObject::FIELD_NAME());
  }
  return nullptr;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::UpdateColoringParameters()
{
  bool usingScalarColoring = false;

  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    const char* colorArrayName = info->Get(vtkDataObject::FIELD_NAME());
    int fieldAssociation = info->Get(vtkDataObject::FIELD_ASSOCIATION());
    if (colorArrayName && colorArrayName[0])
    {
      this->Mapper->SetScalarVisibility(1);
      this->Mapper->SelectColorArray(colorArrayName);
      this->Mapper->SetUseLookupTableScalarRange(1);
      this->Mapper->SetUseAdaptiveDecimation(this->AdaptiveDecimation);
      switch (fieldAssociation)
      {
        case vtkDataObject::FIELD_ASSOCIATION_NONE:
          this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
          // Color entire block by first tuple in the field data
          this->Mapper->SetFieldDataTupleId(0);
          break;

        case vtkDataObject::FIELD_ASSOCIATION_POINTS: // no point data in HTG
        case vtkDataObject::FIELD_ASSOCIATION_CELLS:
        default:
          this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
          break;
      }
      usingScalarColoring = true;
    }
  }

  if (!usingScalarColoring)
  {
    this->Mapper->SetScalarVisibility(0);
    this->Mapper->SelectColorArray(nullptr);
  }

  // Adjust material properties.
  this->Property->SetAmbient(this->Ambient);
  this->Property->SetSpecular(this->Specular);
  this->Property->SetDiffuse(this->Diffuse);

  switch (this->Representation)
  {
    case SURFACE_WITH_EDGES:
      this->Property->SetEdgeVisibility(1);
      this->Property->SetRepresentation(VTK_SURFACE);
      break;

    default:
      this->Property->SetEdgeVisibility(0);
      this->Property->SetRepresentation(this->Representation);
  }
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetVisibility(bool val)
{
  this->Actor->SetVisibility(val);
  this->Superclass::SetVisibility(val);
}

//----------------------------------------------------------------------------
bool vtkHyperTreeGridRepresentation::NeedsOrderedCompositing()
{
  // One would think simply calling `vtkActor::HasTranslucentPolygonalGeometry`
  // should do the trick, however that method relies on the mapper's input
  // having up-to-date data. vtkHyperTreeGridRepresentation needs to determine
  // whether the representation needs ordered compositing in `REQUEST_UPDATE`
  // pass i.e. before the mapper's input is updated. Hence we explicitly
  // determine if the mapper may choose to render translucent geometry.
  if (this->Actor->GetForceOpaque())
  {
    return false;
  }

  if (this->Actor->GetForceTranslucent())
  {
    return true;
  }

  if (auto prop = this->Actor->GetProperty())
  {
    auto opacity = prop->GetOpacity();
    if (opacity > 0.0 && opacity < 1.0)
    {
      return true;
    }
  }

  if (auto texture = this->Actor->GetTexture())
  {
    if (texture->IsTranslucent())
    {
      return true;
    }
  }

  auto colorarrayname = this->GetColorArrayName();
  if (colorarrayname && colorarrayname[0])
  {
    if (this->Mapper->GetColorMode() == VTK_COLOR_MODE_DIRECT_SCALARS)
    {
      // when mapping scalars directly, assume the scalars have an alpha
      // component since we cannot check if that is indeed the case consistently
      // on all ranks without a bit of work.
      return true;
    }

    if (auto lut = this->Mapper->GetLookupTable())
    {
      if (lut->IsOpaque() == 0)
      {
        return true;
      }
    }
  }

  return false;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << "OpenGL HTG Mapper: " << std::endl;
  this->Mapper->PrintSelf(os, indent.GetNextIndent());
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetUseOutline(int vtkNotUsed(val))
{
  vtkWarningMacro("Outline not supported by the HTG Representation.");

  // since geometry filter needs to execute, we need to mark the representation
  // modified.
  this->MarkModified();
}

//****************************************************************************
// Methods merely forwarding parameters to internal objects.
//****************************************************************************

//----------------------------------------------------------------------------
// Forwarded to vtkProperty
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetAmbientColor(double r, double g, double b)
{
  this->Property->SetAmbientColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetBaseColorTexture(vtkTexture* tex)
{
  if (tex)
  {
    tex->UseSRGBColorSpaceOn();
    tex->SetInterpolate(this->InterpolateTextures);
    tex->SetRepeat(this->RepeatTextures);
    tex->SetMipmap(this->UseMipmapTextures);
  }
  this->Property->SetBaseColorTexture(tex);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetColor(double r, double g, double b)
{
  this->Property->SetColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetDiffuseColor(double r, double g, double b)
{
  this->Property->SetDiffuseColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetEdgeColor(double r, double g, double b)
{
  this->Property->SetEdgeColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetEdgeTint(double r, double g, double b)
{
  this->Property->SetEdgeTint(r, g, b);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetEmissiveFactor(double rval, double gval, double bval)
{
  this->Property->SetEmissiveFactor(rval, gval, bval);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetEmissiveTexture(vtkTexture* tex)
{
  if (tex)
  {
    tex->UseSRGBColorSpaceOn();
    tex->SetInterpolate(this->InterpolateTextures);
    tex->SetRepeat(this->RepeatTextures);
    tex->SetMipmap(this->UseMipmapTextures);
  }
  this->Property->SetEmissiveTexture(tex);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetInteractiveSelectionColor(double r, double g, double b)
{
  this->Property->SetSelectionColor(r, g, b, 1.0);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetInterpolation(int val)
{
  this->Property->SetInterpolation(val);
}
//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetLineWidth(double val)
{
  this->Property->SetLineWidth(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetMaterialTexture(vtkTexture* tex)
{
  if (tex)
  {
    tex->UseSRGBColorSpaceOff();
    tex->SetInterpolate(this->InterpolateTextures);
    tex->SetRepeat(this->RepeatTextures);
    tex->SetMipmap(this->UseMipmapTextures);
  }
  this->Property->SetORMTexture(tex);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetMetallic(double val)
{
  this->Property->SetMetallic(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetNormalScale(double val)
{
  this->Property->SetNormalScale(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetNormalTexture(vtkTexture* tex)
{
  if (tex)
  {
    tex->UseSRGBColorSpaceOff();
    tex->SetInterpolate(this->InterpolateTextures);
    tex->SetRepeat(this->RepeatTextures);
    tex->SetMipmap(this->UseMipmapTextures);
  }
  this->Property->SetNormalTexture(tex);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetOcclusionStrength(double val)
{
  this->Property->SetOcclusionStrength(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetOpacity(double val)
{
  this->Property->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetRenderLinesAsTubes(bool val)
{
  this->Property->SetRenderLinesAsTubes(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetRenderPointsAsSpheres(bool val)
{
  this->Property->SetRenderPointsAsSpheres(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetRoughness(double val)
{
  this->Property->SetRoughness(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetSpecularColor(double r, double g, double b)
{
  this->Property->SetSpecularColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetSpecularPower(double val)
{
  this->Property->SetSpecularPower(val);
}

//----------------------------------------------------------------------------
// Actor
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetFlipTextures(bool flip)
{
  vtkInformation* info = this->Actor->GetPropertyKeys();
  info->Remove(vtkProp::GeneralTextureTransform());
  if (flip)
  {
    double mat[] = { 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    info->Set(vtkProp::GeneralTextureTransform(), mat, 16);
  }
  this->Actor->Modified();
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetOrientation(double x, double y, double z)
{
  this->Actor->SetOrientation(x, y, z);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetOrigin(double x, double y, double z)
{
  this->Actor->SetOrigin(x, y, z);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetPickable(int val)
{
  this->Actor->SetPickable(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetPosition(double x, double y, double z)
{
  this->Actor->SetPosition(x, y, z);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetScale(double x, double y, double z)
{
  this->Actor->SetScale(x, y, z);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetTexture(vtkTexture* val)
{
  this->Actor->SetTexture(val);
  if (val)
  {
    val->SetRepeat(this->RepeatTextures);
    val->SetInterpolate(this->InterpolateTextures);
    val->SetMipmap(this->UseMipmapTextures);
  }
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetUserTransform(const double matrix[16])
{
  vtkNew<vtkTransform> transform;
  transform->SetMatrix(matrix);
  this->Actor->SetUserTransform(transform);
}

//----------------------------------------------------------------------------
// Texture
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetRepeatTextures(bool rep)
{
  if (this->Actor->GetTexture())
  {
    this->Actor->GetTexture()->SetRepeat(rep);
  }
  std::map<std::string, vtkTexture*>& tex = this->Actor->GetProperty()->GetAllTextures();
  for (auto t : tex)
  {
    t.second->SetRepeat(rep);
  }
  this->RepeatTextures = rep;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetInterpolateTextures(bool rep)
{
  if (this->Actor->GetTexture())
  {
    this->Actor->GetTexture()->SetInterpolate(rep);
  }
  std::map<std::string, vtkTexture*>& tex = this->Actor->GetProperty()->GetAllTextures();
  for (auto t : tex)
  {
    t.second->SetInterpolate(rep);
  }
  this->InterpolateTextures = rep;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetUseMipmapTextures(bool rep)
{
  if (this->Actor->GetTexture())
  {
    this->Actor->GetTexture()->SetMipmap(rep);
  }
  std::map<std::string, vtkTexture*>& tex = this->Actor->GetProperty()->GetAllTextures();
  for (auto t : tex)
  {
    t.second->SetMipmap(rep);
  }
  this->UseMipmapTextures = rep;
}

//----------------------------------------------------------------------------
// Mapper and LODMapper
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetInterpolateScalarsBeforeMapping(int val)
{
  // XXX This has no effect on HTG as they only have cell data
  this->Mapper->SetInterpolateScalarsBeforeMapping(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetLookupTable(vtkScalarsToColors* val)
{
  this->Mapper->SetLookupTable(val);
}

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetMapScalars(bool val)
{
  this->Mapper->SetColorMode(val ? VTK_COLOR_MODE_MAP_SCALARS : VTK_COLOR_MODE_DIRECT_SCALARS);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetStatic(int val)
{
  this->Mapper->SetStatic(val);
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridRepresentation::SetSelection(vtkSelection* selection)
{
  // we need to shallow copy the existing selection instead of changing it in order to avoid
  // changing the MTime of the mapper to avoid rebuilding everything
  this->Mapper->GetSelection()->ShallowCopy(selection);
}
