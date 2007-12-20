/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#include "cruise/cruise_main.h"
#include "common/file.h"
#include "cruise/cell.h"

namespace Cruise {

cellStruct cellHead;

void resetPtr(cellStruct *ptr) {
	ptr->next = NULL;
	ptr->prev = NULL;
}

void freeMessageList(cellStruct *objPtr) {
/*	if (objPtr) {
		 if (objPtr->next)
		 free(objPtr->next);

		free(objPtr);
	} */
}
void saveCell(Common::OutSaveFile& currentSaveFile) {

	int count = 0;
	cellStruct *t = cellHead.next;

	while(t) {
		count++;
		t = t->next;
	}

	currentSaveFile.writeSint16LE(count);

	t = cellHead.next;
	while(t) {
		char dummy[2] = { 0, 0};

		currentSaveFile.write(dummy, 2);
		currentSaveFile.write(dummy, 2);

		currentSaveFile.writeSint16LE(t->idx);
		currentSaveFile.writeSint16LE(t->type);
		currentSaveFile.writeSint16LE(t->overlay);
		currentSaveFile.writeSint16LE(t->x);
		currentSaveFile.writeSint16LE(t->field_C);
		currentSaveFile.writeSint16LE(t->spriteIdx);
		currentSaveFile.writeSint16LE(t->color);
		currentSaveFile.writeSint16LE(t->backgroundPlane);
		currentSaveFile.writeSint16LE(t->freeze);
		currentSaveFile.writeSint16LE(t->parent);
		currentSaveFile.writeSint16LE(t->parentOverlay);
		currentSaveFile.writeSint16LE(t->parentType);
		currentSaveFile.writeSint16LE(t->followObjectOverlayIdx);
		currentSaveFile.writeSint16LE(t->followObjectIdx);
		currentSaveFile.writeSint16LE(t->animStart);
		currentSaveFile.writeSint16LE(t->animEnd);
		currentSaveFile.writeSint16LE(t->animWait);
		currentSaveFile.writeSint16LE(t->animStep);
		currentSaveFile.writeSint16LE(t->animChange);
		currentSaveFile.writeSint16LE(t->animType);
		currentSaveFile.writeSint16LE(t->animSignal);
		currentSaveFile.writeSint16LE(t->animCounter);
		currentSaveFile.writeSint16LE(t->animLoop);
		currentSaveFile.write(dummy, 2);

		t = t->next;
	}
}

void loadSavegameDataSub2(Common::InSaveFile& currentSaveFile) {
	unsigned short int n_chunks;
	int i;
	cellStruct *p;
	cellStruct *t;

	cellHead.next = NULL;	// Not in ASM code, but I guess the variable is defaulted
	// to this value in the .exe

	n_chunks = currentSaveFile.readSint16LE();

	p = &cellHead;

	for (i = 0; i < n_chunks; i++) {
		t = (cellStruct *) mallocAndZero(sizeof(cellStruct));

		currentSaveFile.skip(2);
		currentSaveFile.skip(2);

		t->idx = currentSaveFile.readSint16LE();
		t->type = currentSaveFile.readSint16LE();
		t->overlay = currentSaveFile.readSint16LE();
		t->x = currentSaveFile.readSint16LE();
		t->field_C = currentSaveFile.readSint16LE();
		t->spriteIdx = currentSaveFile.readSint16LE();
		t->color = currentSaveFile.readSint16LE();
		t->backgroundPlane = currentSaveFile.readSint16LE();
		t->freeze = currentSaveFile.readSint16LE();
		t->parent = currentSaveFile.readSint16LE();
		t->parentOverlay = currentSaveFile.readSint16LE();
		t->parentType = currentSaveFile.readSint16LE();
		t->followObjectOverlayIdx = currentSaveFile.readSint16LE();
		t->followObjectIdx = currentSaveFile.readSint16LE();
		t->animStart = currentSaveFile.readSint16LE();
		t->animEnd = currentSaveFile.readSint16LE();
		t->animWait = currentSaveFile.readSint16LE();
		t->animStep = currentSaveFile.readSint16LE();
		t->animChange = currentSaveFile.readSint16LE();
		t->animType = currentSaveFile.readSint16LE();
		t->animSignal = currentSaveFile.readSint16LE();
		t->animCounter = currentSaveFile.readSint16LE();
		t->animLoop = currentSaveFile.readSint16LE();
		currentSaveFile.skip(2);
		
		t->next = NULL;
		p->next = t;
		t->prev = cellHead.prev;
		cellHead.prev = t;
		p = t;
	}
}

cellStruct *addCell(cellStruct *pHead, int16 overlayIdx, int16 objIdx, int16 type, int16 backgroundPlane, int16 scriptOverlay, int16 scriptNumber, int16 scriptType) {
	int16 var;

	cellStruct *newElement;
	cellStruct *currentHead = pHead;
	cellStruct *currentHead2;
	cellStruct *currentHead3;

	if (getSingleObjectParam(overlayIdx, objIdx, 2, &var) < 0) {
		return 0;
	}

	currentHead3 = currentHead;
	currentHead2 = currentHead->next;

	while (currentHead2) {
		if (currentHead2->type == 3) {
			break;
		}

		if (currentHead2->type != 5) {
			int16 lvar2;

			getSingleObjectParam(currentHead2->overlay, currentHead2->idx, 2, &lvar2);

			if (lvar2 > var)
				break;
		}

		currentHead3 = currentHead2;
		currentHead2 = currentHead2->next;
	}

	if (currentHead2) {
		if ((currentHead2->overlay == overlayIdx) &&
		    (currentHead2->backgroundPlane == backgroundPlane) &&
		    (currentHead2->idx == objIdx) &&
		    (currentHead2->type == type))

			return NULL;
	}

	currentHead = currentHead2;

	newElement = (cellStruct *) mallocAndZero(sizeof(cellStruct));

	if (!newElement)
		return 0;

	newElement->next = currentHead3->next;
	currentHead3->next = newElement;

	newElement->idx = objIdx;
	newElement->type = type;
	newElement->backgroundPlane = backgroundPlane;
	newElement->overlay = overlayIdx;
	newElement->freeze = 0;
	newElement->parent = scriptNumber;
	newElement->parentOverlay = scriptOverlay;
	newElement->gfxPtr = NULL;
	newElement->followObjectIdx = objIdx;
	newElement->followObjectOverlayIdx = overlayIdx;
	newElement->parentType = scriptType;

	newElement->animStart = 0;
	newElement->animEnd = 0;
	newElement->animWait = 0;
	newElement->animSignal = 0;
	newElement->animCounter = 0;
	newElement->animType = 0;
	newElement->animStep = 0;
	newElement->animLoop = 0;

	if (currentHead) {
		newElement->prev = currentHead->prev;
		currentHead->prev = newElement;
	} else {
		newElement->prev = pHead->prev;
		pHead->prev = newElement;
	}

	return newElement;
}

void createTextObject(cellStruct *pObject, int overlayIdx, int messageIdx, int x, int y, int width, int16 color, int backgroundPlane, int parentOvl, int parentIdx) {

	char *ax;
	cellStruct *savePObject = pObject;
	cellStruct *cx;

	cellStruct *pNewElement;
	cellStruct *si = pObject->next;
	cellStruct *var_2;

	while (si) {
		pObject = si;
		si = si->next;
	}

	var_2 = si;

	pNewElement = (cellStruct *) malloc(sizeof(cellStruct));

	pNewElement->next = pObject->next;
	pObject->next = pNewElement;

	pNewElement->idx = messageIdx;
	pNewElement->type = 5;
	pNewElement->backgroundPlane = backgroundPlane;
	pNewElement->overlay = overlayIdx;
	pNewElement->x = x;
	pNewElement->field_C = y;
	pNewElement->spriteIdx = width;
	pNewElement->color = color;
	pNewElement->freeze = 0;
	pNewElement->parent = parentIdx;
	pNewElement->parentOverlay = parentOvl;
	pNewElement->gfxPtr = NULL;

	if (var_2) {
		cx = var_2;
	} else {
		cx = savePObject;
	}

	pNewElement->prev = cx->prev;
	cx->prev = pNewElement;

	ax = getText(messageIdx, overlayIdx);

	if (ax) {
		pNewElement->gfxPtr = renderText(width, (uint8 *) ax);
	}
}

void removeCell(cellStruct *objPtr, int ovlNumber, int objectIdx, int objType, int backgroundPlane ) {
	cellStruct *currentObj = objPtr->next;
	cellStruct *previous;

	while (currentObj) {
		if (((currentObj->overlay == ovlNumber) || (ovlNumber == -1)) &&
		    ((currentObj->idx == objectIdx) || (objectIdx == -1)) &&
		    ((currentObj->type == objType) || (objType == -1)) &&
		    ((currentObj->backgroundPlane == backgroundPlane) || (backgroundPlane == -1))) {
			currentObj->type = -1;
		}

		currentObj = currentObj->next;
	}

	previous = objPtr;
	currentObj = objPtr->next;

	while (currentObj) {
		cellStruct *si;

		si = currentObj;

		if (si->type == -1) {
			cellStruct *dx;
			previous->next = si->next;

			dx = si->next;

			if (!si->next) {
				dx = objPtr;
			}

			dx->prev = si->prev;

			// TODO: complelty wrong
			//freeMessageList(si);

			free(si);

			currentObj = dx;
		} else {
			currentObj = si->next;
			previous = si;
		}
	}
}

void linkCell(cellStruct *pHead, int ovl, int obj, int type, int ovl2, int obj2) {
	while (pHead) {
		if ((pHead->overlay == ovl) || (ovl == -1)) {
			if ((pHead->idx == obj) || (obj == -1)) {
				if ((pHead->type == type) || (type == -1)) {
					pHead->followObjectIdx = obj2;
					pHead->followObjectOverlayIdx = ovl2;
				}
			}
		}

		pHead = pHead->next;
	}
}

void freezeCell(cellStruct * pObject, int overlayIdx, int objIdx, int objType, int backgroundPlane, int oldFreeze, int newFreeze ) {
	while (pObject) {
		if ((pObject->overlay == overlayIdx) || (overlayIdx == -1)) {
			if ((pObject->idx == objIdx) || (objIdx == -1)) {
				if ((pObject->type == objType) || (objType == -1)) {
					if ((pObject->backgroundPlane == backgroundPlane) || (backgroundPlane == -1)) {
						if ((pObject->freeze == oldFreeze) || (oldFreeze == -1)) {
							pObject->freeze = newFreeze;
						}
					}
				}
			}
		}

		pObject = pObject->next;
	}
}

void sortCells(int16 param1, int16 param2, cellStruct *objPtr) {
	cellStruct *pl,*pl2,*pl3,*pl4,*plz,*pllast;
	cellStruct prov;
	int16 newz, objz, sobjz;

	pl4 = NULL;

	getSingleObjectParam(param1, param2, 2, &sobjz);
	pl = objPtr;
	prov.next = NULL;
	prov.prev = NULL;

	pl2 = pl->next;
	pllast = NULL;
	plz = objPtr;

	while (pl2) {
		pl3 = pl2->next;
		if ((pl2->overlay == param1) && (pl2->idx == param2)) {// found
			pl->next = pl3;

			if (pl3) {
				pl3->prev = pl2->prev;
			} else {
				objPtr->prev = pl2->prev;
			}

			pl4 = prov.next;

			if (pl4) {
				pl4->prev = pl2;
			} else {
				prov.prev = pl2;
			}

			pl2->prev = NULL;
			pl2->next = prov.next;
			prov.next = pl2;

			if (pllast == NULL) {
				pllast = pl2;
			}
		} else {
			if (pl2->type == 5) {
				newz = 32000;
			} else {
				getSingleObjectParam(pl2->overlay, pl2->idx, 2, &objz);
				newz = objz;
			}

			if (newz < sobjz) {
				plz = pl2;
			}

			pl = pl->next;
		}

		pl2 = pl3;
	}

	if (pllast) {
		pl2 = prov.next;
		pl4 = plz->next;
		plz->next = pl2;
		pllast->next = pl4;

		if(plz != objPtr)
			pl2->prev = plz;
		if(!pl4)
			objPtr->prev = pllast;
		else
			pl4->prev = pllast;
	}
}

} // End of namespace Cruise
