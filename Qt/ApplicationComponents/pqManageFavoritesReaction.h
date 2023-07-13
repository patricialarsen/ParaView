// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
#ifndef pqManageFavoritesReaction_h
#define pqManageFavoritesReaction_h

#include "pqMasterOnlyReaction.h"

#include "vtkParaViewDeprecation.h" // for deprecation

class QAction;
class pqProxyGroupMenuManager;

/**
 * @ingroup Reactions
 * pqManageFavoritesReaction is the reaction to pop-up the favorites manager dialog.
 */
class PARAVIEW_DEPRECATED_IN_5_13_0(
  "Favorites should be replaced by Categories configuration. See pqConfigureCategories instead.")
  PQAPPLICATIONCOMPONENTS_EXPORT pqManageFavoritesReaction : public pqMasterOnlyReaction
{
  Q_OBJECT
  typedef pqMasterOnlyReaction Superclass;

public:
  pqManageFavoritesReaction(QAction* action, pqProxyGroupMenuManager* mgr)
    : Superclass(action)
    , manager(mgr)
  {
  }

  /**
   * Pops-up the pqFavoriteDialog dialog.
   */
  static void manageFavorites(pqProxyGroupMenuManager* manager);

protected:
  void onTriggered() override { pqManageFavoritesReaction::manageFavorites(this->manager); }

private:
  Q_DISABLE_COPY(pqManageFavoritesReaction)

  pqProxyGroupMenuManager* manager;
};

#endif
