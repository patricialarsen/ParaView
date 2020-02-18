/*=========================================================================

   Program: ParaView
   Module: pqBackgroundEditorWidget.h

   Copyright (c) 2005-2012 Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#ifndef _pqBackgroundEditorWidget_h
#define _pqBackgroundEditorWidget_h

#include "pqApplicationComponentsModule.h"
#include "pqPropertyGroupWidget.h"

class vtkSMProxy;
class vtkSMPropertyGroup;

class PQAPPLICATIONCOMPONENTS_EXPORT pqBackgroundEditorWidget : public pqPropertyGroupWidget
{
public:
  pqBackgroundEditorWidget(vtkSMProxy* smproxy, vtkSMPropertyGroup* smgroup,
    QWidget* parentObject = 0, bool forEnvironment = false);
  ~pqBackgroundEditorWidget() override;

  bool gradientBackground() const;
  void setGradientBackground(bool gradientBackground);
  bool imageBackground() const;
  void setImageBackground(bool imageBackground);
  bool skyboxBackground() const;
  void setSkyboxBackground(bool skyboxBackground);
  bool environmentLighting() const;
  void setEnvironmentLighting(bool envLighting);

signals:
  void gradientBackgroundChanged();
  void imageBackgroundChanged();
  void skyboxBackgroundChanged();
  void environmentLightingChanged();

protected slots:
  void currentIndexChangedBackgroundType(int type);
  void clickedRestoreDefaultColor();
  void clickedRestoreDefaultColor2();

private:
  typedef pqPropertyGroupWidget Superclass;

private:
  void changeColor(const char* propertyName);
  void fireGradientAndImageChanged(int oldType, int newType);

private:
  Q_OBJECT
  Q_PROPERTY(bool gradientBackground READ gradientBackground WRITE setGradientBackground)
  Q_PROPERTY(bool imageBackground READ imageBackground WRITE setImageBackground)
  Q_PROPERTY(bool skyboxBackground READ skyboxBackground WRITE setSkyboxBackground)
  Q_PROPERTY(bool environmentLighting READ environmentLighting WRITE setEnvironmentLighting)

  class pqInternal;
  pqInternal* Internal;
};

#endif
