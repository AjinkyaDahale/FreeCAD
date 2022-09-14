/***************************************************************************
 *   Copyright (c) 2011 Juergen Riegel <FreeCAD@juergen-riegel.net>        *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/Origin.h>
#include <Base/Console.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/Selection.h>
#include <Gui/ViewProvider.h>
#include <Gui/ViewProviderOrigin.h>
#include <Mod/PartDesign/App/FeatureRevolution.h>
#include <Mod/PartDesign/App/FeatureGroove.h>
#include <Mod/PartDesign/App/Body.h>

#include "ui_TaskRevolutionParameters.h"
#include "TaskRevolutionParameters.h"
#include "ReferenceSelection.h"

using namespace PartDesignGui;
using namespace Gui;

/* TRANSLATOR PartDesignGui::TaskRevolutionParameters */

TaskRevolutionParameters::TaskRevolutionParameters(PartDesignGui::ViewProvider* RevolutionView, QWidget *parent)
    : TaskSketchBasedParameters(RevolutionView, parent, "PartDesign_Revolution", tr("Revolution parameters"))
    , ui(new Ui_TaskRevolutionParameters)
    , isGroove(false)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);
    this->groupLayout()->addWidget(proxy);

    //bind property mirrors
    PartDesign::ProfileBased* pcFeat = static_cast<PartDesign::ProfileBased*>(vp->getObject());
    if (pcFeat->isDerivedFrom(PartDesign::Revolution::getClassTypeId())) {
        PartDesign::Revolution* rev = static_cast<PartDesign::Revolution*>(vp->getObject());
        this->propAngle = &(rev->Angle);
        this->propAngle2 = &(rev->Angle2);
        this->propMidPlane = &(rev->Midplane);
        this->propReferenceAxis = &(rev->ReferenceAxis);
        this->propReversed = &(rev->Reversed);
        this->propUpToFace = &(rev->UpToFace);
        ui->revolveAngle->bind(rev->Angle);
        ui->revolveAngle2->bind(rev->Angle2);
    }
    else {
        assert(pcFeat->isDerivedFrom(PartDesign::Groove::getClassTypeId()));
        isGroove = true;
        PartDesign::Groove* rev = static_cast<PartDesign::Groove*>(vp->getObject());
        this->propAngle = &(rev->Angle);
        this->propAngle2 = &(rev->Angle2);
        this->propMidPlane = &(rev->Midplane);
        this->propReferenceAxis = &(rev->ReferenceAxis);
        this->propReversed = &(rev->Reversed);
        this->propUpToFace = &(rev->UpToFace);
        ui->revolveAngle->bind(rev->Angle);
        ui->revolveAngle2->bind(rev->Angle2);
    }

    setupDialog();

    blockUpdate = false;
    updateUI();
    connectSignals();

    setFocus();

    //show the parts coordinate system axis for selection
    PartDesign::Body * body = PartDesign::Body::findBodyOf ( vp->getObject () );
    if(body) {
        try {
            App::Origin *origin = body->getOrigin();
            ViewProviderOrigin* vpOrigin;
            vpOrigin = static_cast<ViewProviderOrigin*>(Gui::Application::Instance->getViewProvider(origin));
            vpOrigin->setTemporaryVisibility(true, false);
        } catch (const Base::Exception &ex) {
            ex.ReportException();
        }
     }
}

void TaskRevolutionParameters::setupDialog()
{
    ui->checkBoxMidplane->setChecked(propMidPlane->getValue());
    ui->checkBoxReversed->setChecked(propReversed->getValue());

    ui->revolveAngle->setValue(propAngle->getValue());
    ui->revolveAngle->setMaximum(propAngle->getMaximum());
    ui->revolveAngle->setMinimum(propAngle->getMinimum());

    int index = 0;

    // TODO: This should also be implemented for groove
    if (!isGroove) {
        PartDesign::Revolution* rev = static_cast<PartDesign::Revolution*>(vp->getObject());
        ui->revolveAngle2->setValue(propAngle2->getValue());
        ui->revolveAngle2->setMaximum(propAngle2->getMaximum());
        ui->revolveAngle2->setMinimum(propAngle2->getMinimum());

        index = rev->Type.getValue();
    }
    else {
        PartDesign::Groove* rev = static_cast<PartDesign::Groove*>(vp->getObject());
        ui->revolveAngle2->setValue(propAngle2->getValue());
        ui->revolveAngle2->setMaximum(propAngle2->getMaximum());
        ui->revolveAngle2->setMinimum(propAngle2->getMinimum());

        index = rev->Type.getValue();
    }

    translateModeList(index);
}

void TaskRevolutionParameters::translateModeList(int index)
{
    ui->changeMode->clear();
    ui->changeMode->addItem(tr("Dimension"));
    if (!isGroove) {
        ui->changeMode->addItem(tr("To last"));
    }
    else {
        ui->changeMode->addItem(tr("Through all"));
    }
    ui->changeMode->addItem(tr("To first"));
    ui->changeMode->addItem(tr("Up to face"));
    ui->changeMode->addItem(tr("Two dimensions"));
    ui->changeMode->setCurrentIndex(index);
}

void TaskRevolutionParameters::fillAxisCombo(bool forceRefill)
{
    bool oldVal_blockUpdate = blockUpdate;
    blockUpdate = true;

    if (axesInList.empty())
        forceRefill = true;//not filled yet, full refill

    if (forceRefill){
        ui->axis->clear();

        for(size_t i = 0; i < axesInList.size(); i++){
            delete axesInList[i];
        }
        this->axesInList.clear();

        //add sketch axes
        PartDesign::ProfileBased* pcFeat = static_cast<PartDesign::ProfileBased*>(vp->getObject());
        Part::Part2DObject* pcSketch = dynamic_cast<Part::Part2DObject*>(pcFeat->Profile.getValue());
        if (pcSketch){
            addAxisToCombo(pcSketch,"V_Axis",QObject::tr("Vertical sketch axis"));
            addAxisToCombo(pcSketch,"H_Axis",QObject::tr("Horizontal sketch axis"));
            for (int i=0; i < pcSketch->getAxisCount(); i++) {
                QString itemText = QObject::tr("Construction line %1").arg(i+1);
                std::stringstream sub;
                sub << "Axis" << i;
                addAxisToCombo(pcSketch,sub.str(),itemText);
            }
        }

        //add part axes
        PartDesign::Body * body = PartDesign::Body::findBodyOf ( pcFeat );
        if (body) {
            try {
                App::Origin* orig = body->getOrigin();
                addAxisToCombo(orig->getX(),"",tr("Base X axis"));
                addAxisToCombo(orig->getY(),"",tr("Base Y axis"));
                addAxisToCombo(orig->getZ(),"",tr("Base Z axis"));
            } catch (const Base::Exception &ex) {
                ex.ReportException();
            }
        }

        //add "Select reference"
        addAxisToCombo(nullptr,std::string(),tr("Select reference..."));
    }//endif forceRefill

    //add current link, if not in list
    //first, figure out the item number for current axis
    int indexOfCurrent = -1;
    App::DocumentObject* ax = propReferenceAxis->getValue();
    const std::vector<std::string> &subList = propReferenceAxis->getSubValues();
    for (size_t i = 0; i < axesInList.size(); i++) {
        if (ax == axesInList[i]->getValue() && subList == axesInList[i]->getSubValues())
            indexOfCurrent = i;
    }
    if (indexOfCurrent == -1  &&  ax) {
        assert(subList.size() <= 1);
        std::string sub;
        if (!subList.empty())
            sub = subList[0];
        addAxisToCombo(ax, sub, getRefStr(ax, subList));
        indexOfCurrent = axesInList.size()-1;
    }

    //highlight current.
    if (indexOfCurrent != -1)
        ui->axis->setCurrentIndex(indexOfCurrent);

    blockUpdate = oldVal_blockUpdate;
}

void TaskRevolutionParameters::addAxisToCombo(App::DocumentObject* linkObj,
                                              std::string linkSubname,
                                              QString itemText)
{
    this->ui->axis->addItem(itemText);
    this->axesInList.push_back(new App::PropertyLinkSub());
    App::PropertyLinkSub &lnk = *(axesInList[axesInList.size()-1]);
    lnk.setValue(linkObj,std::vector<std::string>(1,linkSubname));
}

void TaskRevolutionParameters::connectSignals()
{
    connect(ui->revolveAngle, qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this, &TaskRevolutionParameters::onAngleChanged);
    connect(ui->revolveAngle2, qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this, &TaskRevolutionParameters::onAngle2Changed);
    connect(ui->axis, qOverload<int>(&QComboBox::activated),
            this, &TaskRevolutionParameters::onAxisChanged);
    connect(ui->checkBoxMidplane, &QCheckBox::toggled,
            this, &TaskRevolutionParameters::onMidplane);
    connect(ui->checkBoxReversed, &QCheckBox::toggled,
            this, &TaskRevolutionParameters::onReversed);
    connect(ui->checkBoxUpdateView, &QCheckBox::toggled,
            this, &TaskRevolutionParameters::onUpdateView);
    connect(ui->changeMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TaskRevolutionParameters::onModeChanged);
    connect(ui->buttonFace, &QPushButton::toggled,
            this, &TaskRevolutionParameters::onButtonFace);
    connect(ui->lineFaceName, &QLineEdit::textEdited,
            this, &TaskRevolutionParameters::onFaceName);
}

void TaskRevolutionParameters::updateUI(int /*index*/)
{
    if (blockUpdate)
        return;
    blockUpdate = true;

    fillAxisCombo();

    blockUpdate = false;
}

void TaskRevolutionParameters::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (msg.Type == Gui::SelectionChanges::AddSelection) {
        if (selectionFace) {
            QString refText = onAddSelection(msg);
            if (refText.length() > 0) {
                QSignalBlocker block(ui->lineFaceName);
                ui->lineFaceName->setText(refText);
                ui->lineFaceName->setProperty("FeatureName", QByteArray(msg.pObjectName));
                ui->lineFaceName->setProperty("FaceName", QByteArray(msg.pSubName));
                // Turn off reference selection mode
                ui->buttonFace->setChecked(false);
            }
            else {
                clearFaceName();
            }
        }
        else {
            exitSelectionMode();
            std::vector<std::string> axis;
            App::DocumentObject* selObj;
            if (getReferencedSelection(vp->getObject(), msg, selObj, axis) && selObj) {
                propReferenceAxis->setValue(selObj, axis);

                recomputeFeature();
                updateUI();
            }
        }
    }
    else if (msg.Type == Gui::SelectionChanges::ClrSelection && selectionFace) {
        clearFaceName();
    }
}

void TaskRevolutionParameters::onButtonFace(const bool pressed)
{
    // to distinguish that this is the direction selection
    selectionFace = true;

    // only faces are allowed
    TaskSketchBasedParameters::onSelectReference(pressed ? AllowSelection::FACE : AllowSelection::NONE);
}

void TaskRevolutionParameters::onFaceName(const QString& text)
{
    if (text.isEmpty()) {
        // if user cleared the text field then also clear the properties
        ui->lineFaceName->setProperty("FeatureName", QVariant());
        ui->lineFaceName->setProperty("FaceName", QVariant());
    }
    else {
        // expect that the label of an object is used
        QStringList parts = text.split(QChar::fromLatin1(':'));
        QString label = parts[0];
        QVariant name = objectNameByLabel(label, ui->lineFaceName->property("FeatureName"));
        if (name.isValid()) {
            parts[0] = name.toString();
            QString uptoface = parts.join(QString::fromLatin1(":"));
            ui->lineFaceName->setProperty("FeatureName", name);
            ui->lineFaceName->setProperty("FaceName", setUpToFace(uptoface));
        }
        else {
            ui->lineFaceName->setProperty("FeatureName", QVariant());
            ui->lineFaceName->setProperty("FaceName", QVariant());
        }
    }
}

void TaskRevolutionParameters::translateFaceName()
{
    ui->lineFaceName->setPlaceholderText(tr("No face selected"));
    QVariant featureName = ui->lineFaceName->property("FeatureName");
    if (featureName.isValid()) {
        QStringList parts = ui->lineFaceName->text().split(QChar::fromLatin1(':'));
        QByteArray upToFace = ui->lineFaceName->property("FaceName").toByteArray();
        int faceId = -1;
        bool ok = false;
        if (upToFace.indexOf("Face") == 0) {
            faceId = upToFace.remove(0,4).toInt(&ok);
        }

        if (ok) {
            ui->lineFaceName->setText(QString::fromLatin1("%1:%2%3")
                                      .arg(parts[0])
                                      .arg(tr("Face"))
                                      .arg(faceId));
        }
        else {
            ui->lineFaceName->setText(parts[0]);
        }
    }
}

QString TaskRevolutionParameters::getFaceName(void) const
{
    QVariant featureName = ui->lineFaceName->property("FeatureName");
    if (featureName.isValid()) {
        QString faceName = ui->lineFaceName->property("FaceName").toString();
        return getFaceReference(featureName.toString(), faceName);
    }

    return QString::fromLatin1("None");
}

void TaskRevolutionParameters::clearFaceName()
{
    QSignalBlocker block(ui->lineFaceName);
    ui->lineFaceName->clear();
    ui->lineFaceName->setProperty("FeatureName", QVariant());
    ui->lineFaceName->setProperty("FaceName", QVariant());
}

void TaskRevolutionParameters::onAngleChanged(double len)
{
    propAngle->setValue(len);
    exitSelectionMode();
    recomputeFeature();
}

void TaskRevolutionParameters::onAngle2Changed(double len)
{
    if (propAngle2)
        propAngle2->setValue(len);
    exitSelectionMode();
    recomputeFeature();
}

void TaskRevolutionParameters::onAxisChanged(int num)
{
    if (blockUpdate)
        return;
    PartDesign::ProfileBased* pcRevolution = static_cast<PartDesign::ProfileBased*>(vp->getObject());

    if (axesInList.empty())
        return;

    App::DocumentObject *oldRefAxis = propReferenceAxis->getValue();
    std::vector<std::string> oldSubRefAxis = propReferenceAxis->getSubValues();
    std::string oldRefName;
    if (!oldSubRefAxis.empty())
        oldRefName = oldSubRefAxis.front();

    App::PropertyLinkSub &lnk = *(axesInList[num]);
    if (!lnk.getValue()) {
        // enter reference selection mode
        TaskSketchBasedParameters::onSelectReference(AllowSelection::EDGE |
                                                     AllowSelection::PLANAR |
                                                     AllowSelection::CIRCLE);
    } else {
        if (!pcRevolution->getDocument()->isIn(lnk.getValue())){
            Base::Console().Error("Object was deleted\n");
            return;
        }
        propReferenceAxis->Paste(lnk);
        exitSelectionMode();
    }

    try {
        App::DocumentObject *newRefAxis = propReferenceAxis->getValue();
        const std::vector<std::string> &newSubRefAxis = propReferenceAxis->getSubValues();
        std::string newRefName;
        if (!newSubRefAxis.empty())
            newRefName = newSubRefAxis.front();

        if (oldRefAxis != newRefAxis ||
            oldSubRefAxis.size() != newSubRefAxis.size() ||
            oldRefName != newRefName) {
            bool reversed = propReversed->getValue();
            if (pcRevolution->isDerivedFrom(PartDesign::Revolution::getClassTypeId()))
                reversed = static_cast<PartDesign::Revolution*>(pcRevolution)->suggestReversed();
            if (pcRevolution->isDerivedFrom(PartDesign::Groove::getClassTypeId()))
                reversed = static_cast<PartDesign::Groove*>(pcRevolution)->suggestReversed();

            if (reversed != propReversed->getValue()) {
                propReversed->setValue(reversed);
                ui->checkBoxReversed->blockSignals(true);
                ui->checkBoxReversed->setChecked(reversed);
                ui->checkBoxReversed->blockSignals(false);
            }
        }

        recomputeFeature();
    }
    catch (const Base::Exception& e) {
        e.ReportException();
    }
}

void TaskRevolutionParameters::onMidplane(bool on)
{
    propMidPlane->setValue(on);
    recomputeFeature();
}

void TaskRevolutionParameters::onReversed(bool on)
{
    propReversed->setValue(on);
    recomputeFeature();
}

void TaskRevolutionParameters::onModeChanged(int index)
{
    App::PropertyEnumeration* pcType;
    if (!isGroove)
        pcType = &(static_cast<PartDesign::Revolution*>(vp->getObject())->Type);
    else
        pcType = &(static_cast<PartDesign::Groove*>(vp->getObject())->Type);

    switch (static_cast<PartDesign::Revolution::RevolMethod>(index)) {
    case PartDesign::Revolution::RevolMethod::Dimension:
        pcType->setValue("Angle");
        // Avoid error message
        // if (ui->revolveAngle->value() < Base::Quantity(Precision::Angular(), Base::Unit::Angle)) // TODO: Ensure radians/degree consistency
        //     ui->revolveAngle->setValue(5.0);
        break;
    case PartDesign::Revolution::RevolMethod::ToLast:
        if (!isGroove)
        pcType->setValue("UpToLast");
        else
            pcType->setValue("ThroughAll");
        break;
    case PartDesign::Revolution::RevolMethod::ToFirst:
        pcType->setValue("UpToFirst");
        break;
    case PartDesign::Revolution::RevolMethod::ToFace:
        pcType->setValue("UpToFace");
        break;
    case PartDesign::Revolution::RevolMethod::TwoDimensions:
        pcType->setValue("TwoAngles");
        break;
    }

    updateUI(index);
    recomputeFeature();
}

double TaskRevolutionParameters::getAngle() const
{
    return ui->revolveAngle->value().getValue();
}

void TaskRevolutionParameters::getReferenceAxis(App::DocumentObject*& obj, std::vector<std::string>& sub) const
{
    if (axesInList.empty())
        throw Base::RuntimeError("Not initialized!");

    int num = ui->axis->currentIndex();
    const App::PropertyLinkSub &lnk = *(axesInList[num]);
    if (!lnk.getValue()) {
        throw Base::RuntimeError("Still in reference selection mode; reference wasn't selected yet");
    } else {
        PartDesign::ProfileBased* pcRevolution = static_cast<PartDesign::ProfileBased*>(vp->getObject());
        if (!pcRevolution->getDocument()->isIn(lnk.getValue())){
            throw Base::RuntimeError("Object was deleted");
        }

        obj = lnk.getValue();
        sub = lnk.getSubValues();
    }
}

bool TaskRevolutionParameters::getMidplane() const
{
    return ui->checkBoxMidplane->isChecked();
}

bool TaskRevolutionParameters::getReversed() const
{
    return ui->checkBoxReversed->isChecked();
}

TaskRevolutionParameters::~TaskRevolutionParameters()
{
    try {
        //hide the parts coordinate system axis for selection
        PartDesign::Body * body = vp ? PartDesign::Body::findBodyOf(vp->getObject()) : nullptr;
        if (body) {
            App::Origin *origin = body->getOrigin();
            ViewProviderOrigin* vpOrigin;
            vpOrigin = static_cast<ViewProviderOrigin*>(Gui::Application::Instance->getViewProvider(origin));
            vpOrigin->resetTemporaryVisibility();
        }
    } catch (const Base::Exception &ex) {
        ex.ReportException();
    }

    for (size_t i = 0; i < axesInList.size(); i++) {
        delete axesInList[i];
    }
}

void TaskRevolutionParameters::changeEvent(QEvent *e)
{
    TaskBox::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(proxy);

        // Translate mode items
        translateModeList(ui->changeMode->currentIndex());
    }
}

void TaskRevolutionParameters::apply()
{
    //Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Revolution changed"));
    ui->revolveAngle->apply();
    ui->revolveAngle2->apply();
    std::vector<std::string> sub;
    App::DocumentObject* obj;
    getReferenceAxis(obj, sub);
    std::string axis = buildLinkSingleSubPythonStr(obj, sub);
    auto tobj = vp->getObject();
    FCMD_OBJ_CMD(tobj, "ReferenceAxis = " << axis);
    FCMD_OBJ_CMD(tobj, "Midplane = " << (getMidplane() ? 1 : 0));
    FCMD_OBJ_CMD(tobj, "Reversed = " << (getReversed() ? 1 : 0));
    int mode = ui->changeMode->currentIndex();
    FCMD_OBJ_CMD(tobj, "Type = " << mode);
    QString facename = QString::fromLatin1("None");
    if (static_cast<PartDesign::Revolution::RevolMethod>(mode) == PartDesign::Revolution::RevolMethod::ToFace) {
        facename = getFaceName();
    }
    FCMD_OBJ_CMD(tobj, "UpToFace = " << facename.toLatin1().data());
}

//**************************************************************************
//**************************************************************************
// TaskDialog
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
TaskDlgRevolutionParameters::TaskDlgRevolutionParameters(PartDesignGui::ViewProvider *RevolutionView)
    : TaskDlgSketchBasedParameters(RevolutionView)
{
    assert(RevolutionView);
    Content.push_back(new TaskRevolutionParameters(RevolutionView));
}


#include "moc_TaskRevolutionParameters.cpp"
