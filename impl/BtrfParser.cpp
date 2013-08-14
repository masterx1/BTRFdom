/*
 * BTRFdom - Rappelz BTRF Document Object Model
 * By Glandu2/Ldxngx
 * Copyright 2013 Ldxngx
 *
 * This file is part of BTRFdom.
 * BTRFdom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BTRFdom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BTRFdom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BtrfParser.h"
#include "BtrfRootBlock.h"
#include "BtrfBlock.h"
#include <iostream>
#include <assert.h>

#include "TmlFile.h"
#include "TmlBlock.h"

BtrfRootBlock *BtrfParser::readFile(const char* filename) {
	int blockSize, nextHeaderPos;

	static_assert(sizeof(TemplateGuid) == 16, "Wrong type size");
	static_assert(sizeof(char) == 1, "Wrong type size");
	static_assert(sizeof(short) == 2, "Wrong type size");
	static_assert(sizeof(int) == 4, "Wrong type size");
	static_assert(sizeof(float) == 4, "Wrong type size");
	static_assert(sizeof(GlobalHeader) == 12, "Wrong type size");

	file = new File;
	if(file->open(filename) == false)
		return 0;

	header = file->read<GlobalHeader>(sizeof(*header));

	if(strncmp(header->btrf, "BTRF", 4) || header->header_size != 4) {
		std::cerr << "Invalid BTRF file, missing signature\n";
		return nullptr;
	}
	if(header->major_version != 1 || header->minor_version != 1) {
		std::cerr << "Unsupported BTRF version " << header->major_version << '.' << header->minor_version << ", only version 1.1 si supported\n";
		return nullptr;
	}


	rootBlock = new BtrfRootBlock(tmlData);

	nextHeaderPos = header->header_size + 8;

	//Read template list
	file->setPos(nextHeaderPos);
	blockSize = *file->read<int>(4);
	nextHeaderPos += blockSize + 4;

	int templateNum = blockSize/18;
	const TemplateGuid *guids = file->read<TemplateGuid>(16*templateNum);
	const short *fieldNumbers = file->read<short>(2*templateNum);

	for(int i = 0; i < templateNum; i++) {
		rootBlock->addTemplate(guids[i], fieldNumbers[i]);
	}

	//Read string list
	file->setPos(nextHeaderPos);
	blockSize = *file->read<int>(4);
	nextHeaderPos += blockSize + 4;

	int stringNum = *file->read<int>(4);
	for(int i = 0; i < stringNum; i++) {
		const char *currentString = file->read<char>(0);
		rootBlock->addString(currentString);
		file->read<void>(strlen(currentString)+1);
	}

	//Read blocks
	file->setPos(nextHeaderPos);
	blockSize = *file->read<int>(4);
	nextHeaderPos += blockSize + 4;

	int numBlocks = *file->read<int>(4);
	char type;
	short templateIndex;
	for(int i = 0; i<numBlocks; i++) {
		file->read<void>(3);
		type = *file->read<char>(1);
		assert(type == 9);	//Actually we know only about template root blocks
		file->read<int>(4);	//blockSize
		templateIndex = *file->read<short>(2) - 1;

		BtrfBlock *block = new BtrfBlock;
		block->setTemplateId(templateIndex);

		TmlBlock *templateField = rootBlock->getTmlFile()->getTemplate(rootBlock->getTemplateGuid(templateIndex));
		templateField->setFieldCount(rootBlock->getTemplateUsedField(templateIndex));

		if(!readBlock(block, templateField)) {
			std::cerr << "Premature end of file\n";
			break;
		}
		rootBlock->addBlock(block);
	}

	return rootBlock;
}

BtrfBlock *BtrfParser::readBlock(BtrfBlock *block, TmlBlock *tmlField) {
	int blockSize;
	int i;
	short templateIndex;

	ElementType elementType = tmlField->getType();
	block->construct(tmlField, rootBlock);
	block->setElementNumber(block->getFieldInfo()->getFieldCount());

	if(elementType == ET_TemplateArray || block->getFieldInfo()->getHasVariableSize())
		blockSize = *file->read<int>(4);

	switch(elementType) {
	case ET_TemplateArray:
		if(block->getFieldInfo()->getHasVariableSize()) {
			templateIndex = *file->read<short>(2) - 1;

			block->setTemplateId(templateIndex);
			TmlBlock *templateField = block->getFieldInfo()->getField(0);
			templateField->setFieldCount(rootBlock->getTemplateUsedField(templateIndex));

			block->setElementNumber(*file->read<int>(4));

			block->setData(ET_TemplateArray);
			for(i = 0; i < block->getElementNumber(); i++) {
				if(templateField->getHasVariableSize())
					file->read<int>(4);	//elementSize
				readBlock(block->getBlock(i), templateField);
			}
		} else {
			block->setData(ET_TemplateArray);
			for(i = 0; i < block->getElementNumber(); i++)
				readBlock(block->getBlock(i), block->getFieldInfo()->getField(0));
		}
		break;

	case ET_Template:
		block->setData(ET_Template);

		for(i = 0; i < block->getElementNumber(); i++) {
			readBlock(block->getBlock(i), block->getFieldInfo()->getField(i));
		}
		break;

	case ET_Char:
	case ET_UChar:
		if(block->getFieldInfo()->getHasVariableSize()) {
			file->read<char>(1);	//type
			block->setElementNumber((blockSize-1) / sizeof(char));
		}
		block->setDataPtr(elementType, const_cast<void*>(file->read<void>(block->getElementNumber()*sizeof(char))));
		break;

	case ET_Word:
		if(block->getFieldInfo()->getHasVariableSize()) {
			file->read<char>(1);	//type
			block->setElementNumber((blockSize-1) / sizeof(short));
		}
		block->setDataPtr(elementType, const_cast<void*>(file->read<void>(block->getElementNumber()*sizeof(short))));
		break;

	case ET_DWord:
		if(block->getFieldInfo()->getHasVariableSize()) {
			file->read<char>(1);	//type
			block->setElementNumber((blockSize-1) / sizeof(int));
		}
		block->setDataPtr(elementType, const_cast<void*>(file->read<void>(block->getElementNumber()*sizeof(int))));
		break;

	case ET_Float:
		if(block->getFieldInfo()->getHasVariableSize()) {
			file->read<char>(1);	//type
			block->setElementNumber((blockSize-1) / sizeof(float));
		}
		block->setDataPtr(elementType, const_cast<void*>(file->read<void>(block->getElementNumber()*sizeof(float))));
		break;

	case ET_String:
		if(block->getFieldInfo()->getHasVariableSize()) {
			file->read<char>(1);	//type
			block->setElementNumber((blockSize-1) / sizeof(int));
		}
		block->setData(elementType);
		for(i = 0; i < block->getElementNumber(); i++) {
			int stringId = *file->read<int>(4);
			if(stringId == 0)
				block->getDataPtr<int*>()[i] = -1;
			else {
				block->getDataPtr<int*>()[i] = stringId-1;
			}
		}
		break;

	case ET_None:
	case ET_Array:
		fprintf(stderr, "Invalid type None or Array, ignoring\n");
		break;
	}

	return block;
}


void BtrfParser::writeFile(const char* filename, IBtrfRootBlock *iRootBlock) {
	FILE *file;
	GlobalHeader header;
	int value;
	int blockSizePosition;
	BtrfRootBlock *rootBlock = static_cast<BtrfRootBlock*>(iRootBlock);

	file = fopen(filename, "wb");
	if(!file)
		return;

	header.btrf[0] = 'B';
	header.btrf[1] = 'T';
	header.btrf[2] = 'R';
	header.btrf[3] = 'F';
	header.header_size = 4;
	header.major_version = 1;
	header.minor_version = 1;
	fwrite(&header, 1, sizeof(header), file);

	value = rootBlock->getTemplateNum();
	value *= 18;

	//Padding
	if(value % 4)
		value += 4 - (value % 4);
	fwrite(&value, 1, 4, file);
	for(int i = 0; i < rootBlock->getTemplateNum(); i++) {
		TemplateGuid guid = rootBlock->getTemplateGuid(i);
		fwrite(&guid, 1, sizeof(TemplateGuid), file);
	}
	for(int i = 0; i < rootBlock->getTemplateNum(); i++) {
		short usedFields = rootBlock->getTemplateUsedField(i);
		fwrite(&usedFields, 1, sizeof(short), file);
	}

	//Padding
	value = '1';
	while(ftell(file) % 4) {
		fwrite(&value, 1, 1, file);
		value++;
	}

	blockSizePosition = ftell(file);
	value = 0;
	fwrite(&value, 1, 4, file);  //real block size will be written after

	value = rootBlock->getStringNum();
	fwrite(&value, 1, 4, file);
	for(int i = 0; i < rootBlock->getStringNum(); i++) {
		const char *str = rootBlock->getString(i);
		fwrite(str, strlen(str)+1, 1, file);
	}
	//wstring, not supported
	value = 0;
	fwrite(&value, 1, 4, file);

	//Padding
	value = '1';
	while(ftell(file) % 4) {
		fwrite(&value, 1, 1, file);
		value++;
	}
	value = ftell(file) - blockSizePosition - 4;
	fseek(file, blockSizePosition, SEEK_SET);
	fwrite(&value, 1, 4, file);
	fseek(file, value, SEEK_CUR);

	//data
	blockSizePosition = ftell(file);
	value = 0;
	fwrite(&value, 1, 4, file);


	//write data blocks
	value = rootBlock->getBlockNum();
	fwrite(&value, 1, 4, file);
	for(int i = 0; i < rootBlock->getBlockNum(); i++) {
		int subBlockSizePosition;
		BtrfBlock *currentBlock = rootBlock->getBlock(i);

		//Dummy + type
		value = 0x09000000;  //3 null bytes + 0x9 as type
		fwrite(&value, 1, 4, file);

		//Block size (latter)
		subBlockSizePosition = ftell(file);
		value = 0;
		fwrite(&value, 1, 4, file);

		value = currentBlock->getTemplateId() + 1;
		fwrite(&value, 1, 2, file);

		writeBlock(file, currentBlock);

		value = ftell(file) - subBlockSizePosition - 4;
		fseek(file, subBlockSizePosition, SEEK_SET);
		fwrite(&value, 1, 4, file);
		fseek(file, value, SEEK_CUR);
	}

	value = ftell(file) - blockSizePosition - 4;
	fseek(file, blockSizePosition, SEEK_SET);
	fwrite(&value, 1, 4, file);
}

void BtrfParser::writeBlock(FILE* file, BtrfBlock *block) {
	int i, value;
	int blockSizePosition;


	ElementType elementType = block->getType();

	if(elementType == ET_TemplateArray || block->getFieldInfo()->getHasVariableSize()) {
		blockSizePosition = ftell(file);
		value = 0;
		fwrite(&value, 1, 4, file);
	}

	switch(elementType) {
	case ET_TemplateArray:
		if(block->getFieldInfo()->getHasVariableSize()) {
			value = block->getTemplateId() + 1;
			fwrite(&value, 1, 2, file);

			value = block->getElementNumber();
			fwrite(&value, 1, 4, file);

			for(i = 0; i < block->getElementNumber(); i++) {
				int elementSizePosition = 0;
				BtrfBlock *subBlock = block->getBlock(i);

				if(subBlock->getFieldInfo()->getHasVariableSize()) {
					elementSizePosition = ftell(file);
					value = 0;
					fwrite(&value, 1, 4, file);
				}

				writeBlock(file, subBlock);

				if(subBlock->getFieldInfo()->getHasVariableSize()) {
					value = ftell(file) - elementSizePosition - 4;
					fseek(file, elementSizePosition, SEEK_SET);
					fwrite(&value, 1, 4, file);
					fseek(file, value, SEEK_CUR);
				}
			}
		} else {
			for(i = 0; i < block->getElementNumber(); i++)
				writeBlock(file, block->getBlock(i));
		}
		break;

	case ET_Template:
		for(i = 0; i < block->getElementNumber(); i++) {
			writeBlock(file, block->getBlock(i));
		}
		break;

	case ET_Char:
	case ET_UChar:
		if(block->getFieldInfo()->getHasVariableSize()) {
			value = ET_Char;
			fwrite(&value, 1, 1, file);
		}
		fwrite(block->getDataPtr<char*>(), sizeof(char), block->getElementNumber(), file);
		break;

	case ET_Word:
		if(block->getFieldInfo()->getHasVariableSize()) {
			value = elementType;
			fwrite(&value, 1, 1, file);
		}
		fwrite(block->getDataPtr<short*>(), sizeof(short), block->getElementNumber(), file);
		break;

	case ET_DWord:
		if(block->getFieldInfo()->getHasVariableSize()) {
			value = elementType;
			fwrite(&value, 1, 1, file);
		}
		fwrite(block->getDataPtr<int*>(), sizeof(int), block->getElementNumber(), file);
		break;

	case ET_Float:
		if(block->getFieldInfo()->getHasVariableSize()) {
			value = elementType;
			fwrite(&value, 1, 1, file);
		}
		fwrite(block->getDataPtr<float*>(), sizeof(float), block->getElementNumber(), file);
		break;

	case ET_String:
		if(block->getFieldInfo()->getHasVariableSize()) {
			value = elementType;
			fwrite(&value, 1, 1, file);
		}
		for(i = 0; i < block->getElementNumber(); i++) {
			value = block->getData<int>(i)+1;
			fwrite(&value, 1, 4, file);
		}
		break;

	case ET_None:
	case ET_Array:
		fprintf(stderr, "Invalid type None or Array, ignoring\n");
		break;
	}

	if(elementType == ET_TemplateArray || block->getFieldInfo()->getHasVariableSize()) {
		value = ftell(file) - blockSizePosition - 4;
		fseek(file, blockSizePosition, SEEK_SET);
		fwrite(&value, 1, 4, file);
		fseek(file, value, SEEK_CUR);
	}
}
