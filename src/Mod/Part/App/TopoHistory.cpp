/***************************************************************************
 *   Copyright (c) Ajinkya Dahale       (ajinkyadahale@gmail.com) 2017     *
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

#include <TopTools_ListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <Standard_Failure.hxx>
#include <TDF_Data.hxx>
#include <TNaming_Builder.hxx>
#include <TNaming_Tool.hxx>
#include <TNaming_Builder.hxx>
#include <TNaming_Selector.hxx>
#include <TNaming_NamedShape.hxx>
#include <TNaming_UsedShapes.hxx>
#include <TopExp_Explorer.hxx>

#include "TopoHistory.h"
#include "TopoShape.h"

using namespace Part;

TYPESYSTEM_SOURCE(Part::TopoHistory, Base::BaseClass)

TopoHistory::TopoHistory(): dataFW(new TDF_Data())
{
}

TopoHistory::TopoHistory(const TopoHistory &history)
    : shapeMaker(history.shapeMaker), dataFW(history.dataFW)
{
}

void TopoHistory::operator =(const TopoHistory &history)
{
    if (this != &history) {
        this->shapeMaker = history.shapeMaker;
    }
}

std::vector<TopoShape> TopoHistory::modified(const TopoShape &oldShape)
{
    std::vector<TopoShape> newShapes;
    if (this->shapeMaker.get()) {
        TopoDS_Shape _shape = oldShape.getShape();
        const TopTools_ListOfShape& _newShapes = this->shapeMaker->Modified(_shape);
        for(TopTools_ListIteratorOfListOfShape it(_newShapes); it.More(); it.Next()){
            newShapes.push_back(TopoShape(it.Value()));
        }
        return newShapes;
    }
    Standard_Failure::Raise("History is empty");
    return newShapes; // just to silence compiler warning
}

std::vector<TopoShape> TopoHistory::generated(const TopoShape &oldShape)
{
    std::vector<TopoShape> newShapes;
    if (this->shapeMaker.get()) {
        TopoDS_Shape _shape = oldShape.getShape();
        const TopTools_ListOfShape& _newShapes = this->shapeMaker->Modified(_shape);
        for(TopTools_ListIteratorOfListOfShape it(_newShapes); it.More(); it.Next()){
            newShapes.push_back(TopoShape(it.Value()));
        }
        return newShapes;
    }
    Standard_Failure::Raise("History is empty");
    return newShapes; // just to silence compiler warning
}


bool TopoHistory::isDeleted(const TopoShape &oldShape)
{
    if (this->shapeMaker.get()) {
        TopoDS_Shape _shape = oldShape.getShape();
        return this->shapeMaker->IsDeleted(_shape);
    }
    Standard_Failure::Raise("History is empty");
    return false; // just to silence compiler warning
}

void TopoHistory::buildHistory(const std::shared_ptr<BRepBuilderAPI_MakeShape> &mkShape,
                               TopAbs_ShapeEnum shType, const TopoDS_Shape &oldS,
                               const TopoDS_Shape &newS)
{
    TDF_Label rootL = dataFW->Root();

    TDF_Label baseLabel = TDF_TagSource::NewChild(rootL);
    TNaming_Builder baseBuilder(baseLabel);
    baseBuilder.Generated(oldS);

    TopExp_Explorer baseEx(oldS, shType);
    for (; baseEx.More(); baseEx.Next()) {
        TDF_Label faceLabel = TDF_TagSource::NewChild(baseLabel);
        TNaming_Builder faceBuilder(faceLabel);
        faceBuilder.Generated(baseEx.Current());
    }

    TDF_Label modLabel = TDF_TagSource::NewChild(rootL);
    TNaming_Builder modBuilder(modLabel);
    modBuilder.Modify(oldS, newS);

    baseEx.ReInit();
    TDF_Label modifFacesLabel = TDF_TagSource::NewChild(modLabel);
    TDF_Label delFacesLabel = TDF_TagSource::NewChild(modLabel);
    TNaming_Builder modifFacesBuilder(modifFacesLabel);
    TNaming_Builder delFacesBuilder(delFacesLabel);

    for (; baseEx.More(); baseEx.Next()) {
        TopoDS_Shape oldSubShape = baseEx.Current(), newSubShape;
        if (mkShape->IsDeleted(oldSubShape)) {
            delFacesBuilder.Delete(oldSubShape);
            continue;
        }
        for (TopTools_ListIteratorOfListOfShape it(mkShape->Modified(oldSubShape));
             it.More(); it.Next()) {
            newSubShape = it.Value();
            if (!oldSubShape.IsSame(newSubShape)) {
                modifFacesBuilder.Modify(oldSubShape, newSubShape);
            }
        }
    }
}
