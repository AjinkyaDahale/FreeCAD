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

#include <TopTools_IndexedMapOfShape.hxx>
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

#include "TopoParaHistory.h"
#include "TopoShape.h"

using namespace Part;

TYPESYSTEM_SOURCE(Part::TopoParaHistory, Base::BaseClass)

TopoParaHistory::TopoParaHistory(): dataFW(new TDF_Data())
{
}

TopoParaHistory::TopoParaHistory(const TopoParaHistory &history)
    : shapeMaker(history.shapeMaker), dataFW(history.dataFW)
{
}

void TopoParaHistory::operator =(const TopoParaHistory &history)
{
    if (this != &history) {
        this->shapeMaker = history.shapeMaker;
    }
}

std::vector<TopoShape> TopoParaHistory::modified(const TopoShape &oldShape, const TopoShape &context)
{
    std::vector<TopoShape> newShapes;
//    if (this->shapeMaker.get()) {
//        TopoDS_Shape _shape = oldShape.getShape();
//        const TopTools_ListOfShape& _newShapes = this->shapeMaker->Modified(_shape);
//        for(TopTools_ListIteratorOfListOfShape it(_newShapes); it.More(); it.Next()){
//            newShapes.push_back(TopoShape(it.Value()));
//        }
//        return newShapes;
//    }
//    Standard_Failure::Raise("History is empty");
    TDF_Label selLabel = TDF_TagSource::NewChild(dataFW->Root());
    TNaming_Selector selector(selLabel);
    selector.Select(oldShape, context);
    TDF_LabelMap scope;
    Standard_Boolean solved = selector.Solve(scope);

    if (solved) {
        newShapes.push_back(
                    TopoShape(
                        TNaming_Tool::GetShape(selector.NamedShape())));
        return newShapes;
    }
    Standard_Failure::Raise("Selector::Solve failed");
    return newShapes; // just to silence compiler warning
}

std::vector<TopoShape> TopoParaHistory::generated(const TopoShape &oldShape)
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

bool TopoParaHistory::isDeleted(const TopoShape &oldShape)
{
    if (this->shapeMaker.get()) {
        TopoDS_Shape _shape = oldShape.getShape();
        return this->shapeMaker->IsDeleted(_shape);
    }
    Standard_Failure::Raise("History is empty");
    return false; // just to silence compiler warning
}

void TopoParaHistory::buildHistory(const std::shared_ptr<BRepBuilderAPI_MakeShape> &mkShape,
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

void TopoParaHistory::buildHistory(const TopoDS_Shape &oldS, const TopoDS_Shape &newS,
                                   const std::vector<TopoDS_Shape> &oldSubShapes,
                                   const std::vector<TopoDS_Shape> &newSubShapes)
{
    TDF_Label rootL = dataFW->Root();

    TDF_Label baseLabel = TDF_TagSource::NewChild(rootL);
    TNaming_Builder baseBuilder(baseLabel);
    baseBuilder.Generated(oldS);

    for (std::vector<TopoDS_Shape>::const_iterator it = oldSubShapes.begin();
         it != oldSubShapes.end(); ++it) {
        TDF_Label faceLabel = TDF_TagSource::NewChild(baseLabel);
        TNaming_Builder faceBuilder(faceLabel);
        faceBuilder.Generated(*it);
    }

    TDF_Label modLabel = TDF_TagSource::NewChild(rootL);
    TNaming_Builder modBuilder(modLabel);
    modBuilder.Modify(oldS, newS);

    TDF_Label modifFacesLabel = TDF_TagSource::NewChild(modLabel);
    TDF_Label delFacesLabel = TDF_TagSource::NewChild(modLabel);
    TNaming_Builder modifFacesBuilder(modifFacesLabel);
    TNaming_Builder delFacesBuilder(delFacesLabel);

    for (std::vector<TopoDS_Shape>::const_iterator it1 = oldSubShapes.begin(),
         it2 = newSubShapes.begin();
         it1 != oldSubShapes.end(); ++it1, ++it2) {
        if (it2->IsNull()) {
            delFacesBuilder.Delete(*it1);
            continue;
        }
        if (!it1->IsSame(*it2)) {
            modifFacesBuilder.Modify(*it1, *it2);
        }
    }
}
