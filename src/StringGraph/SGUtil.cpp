//-----------------------------------------------
// Copyright 2009 Wellcome Trust Sanger Institute
// Written by Jared Simpson (js18@sanger.ac.uk)
// Released under the GPL license
//-----------------------------------------------
//
// SGUtils - Data structures/Functions related
// to building and manipulating string graphs
//
#include "SGUtil.h"
#include "SeqReader.h"

ContainMap::ContainMap(std::string file)
{
	std::ifstream reader(file.c_str());
	checkFileHandle(reader, file);
	Overlap o;
	while(reader >> o)
	{
		// By convention the lexographically lower ID is the container and the lexohigher is the contained
		if(o.id[0] < o.id[1])
			add(o.id[1], o.id[0]);
		else
			add(o.id[0], o.id[1]);
	}
	reader.close();
}

void ContainMap::add(std::string s1, std::string s2)
{
	/*
	if(isContained(s1))
		std::cerr << "Warning: " << s1 << " is contained in: " << getContainer(s1) << "\n";
	if(isContained(s2))
		std::cerr << "Warning: " << s2 << " is contained in: " << getContainer(s2) << "\n";
	*/
	assert(s2 < s1);
	m_data.insert(std::make_pair(s1, s2));
}

bool ContainMap::isContained(std::string s) const
{
	StrStrMap::const_iterator iter = m_data.find(s);
	return iter != m_data.end();
}

std::string ContainMap::getContainer(std::string s) const
{
	StrStrMap::const_iterator iter = m_data.find(s);
	if(iter != m_data.end())
		return iter->second;
	else
		return "";
}

// Construct a string graph from overlaps
StringGraph* createSGFromOverlaps(std::string readFile, std::string overlapFile, std::string containFile)
{
	// Initialize the string graph
	StringGraph* pGraph = new StringGraph;

	// Load the containment mappings
	ContainMap containments(containFile);
	
	// Create the graph
	createVertices(pGraph, readFile, containments);
	createEdges(pGraph, overlapFile, containments);
	return pGraph;
}

void createVertices(StringGraph* pGraph, std::string readFile, const ContainMap& containments)
{
	// Add the reads as the vertices
	SeqReader reader(readFile);
	SeqItem si;
	while(reader.get(si))
	{
		if(!containments.isContained(si.id))
		{
			pGraph->addVertex(new StringVertex(si.id, si.seq.toString()));
		}
	}
}

void createEdges(StringGraph* pGraph, std::string overlapFile, const ContainMap& containments)
{
	// Add the overlaps as edges
	std::ifstream overlapReader(overlapFile.c_str());
	checkFileHandle(overlapReader, overlapFile);

	Overlap o;
	while(overlapReader >> o)
	{
		if(containments.isContained(o.id[0]) || containments.isContained(o.id[1]))
		{
			//std::cerr << "skipping edge that has contained vertex " << o << "\n";
			continue;
		}

		// Initialize data and perform checks
		StringVertex* pVerts[2];
		Sequence overhangs[2];
		EdgeComp comp = (o.match.coord[0].isReverse() || o.match.coord[1].isReverse()) ? EC_REVERSE : EC_SAME;

		for(size_t idx = 0; idx < 2; ++idx)
		{
			pVerts[idx] = static_cast<StringVertex*>(pGraph->getVertex(o.id[idx]));
			assert(pVerts[idx]);
			
			// Ensure the reads are not identical
			assert(!o.match.coord[idx].isContained() && o.match.coord[idx].isExtreme());

			std::string overhang = getOverhangString(o.match.coord[idx], pVerts[idx]->getSeq());
			overhangs[idx] = (comp == EC_SAME) ? overhang : reverseComplement(overhang);
		}

		// Add edges
		StringEdge* pEdges[2];
		for(size_t idx = 0; idx < 2; ++idx)
		{
			EdgeDir dir = o.match.coord[idx].isLeftExtreme() ? ED_ANTISENSE : ED_SENSE;
			pEdges[idx] = new StringEdge(pVerts[idx], pVerts[1 - idx], dir, comp, overhangs[1 - idx]);
		}

		pEdges[0]->setTwin(pEdges[1]);
		pEdges[1]->setTwin(pEdges[0]);

		pGraph->addEdge(pEdges[0]);
		pGraph->addEdge(pEdges[1]);
	}
	overlapReader.close();
}

//
std::string getOverhangString(const SeqCoord& sc, const std::string& seq)
{
	size_t left, right;

	int lower;
	int upper;
	if(!sc.isReverse())
	{
		lower = sc.interval.start;
		upper = sc.interval.end;
	}
	else
	{
		lower = sc.interval.end;
		upper = sc.interval.start;
	}

	if(sc.isLeftExtreme())
	{
		 // the end coordinate includes the last base, so the overhang starts at the next pos
		left = upper + 1;
		right = sc.seqlen;
	}
	else
	{
		left = 0;
		right = lower;
	}

	assert(left <= right);
	return seq.substr(left, right - left);
}
