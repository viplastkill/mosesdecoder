/*
 * Sentence.cpp
 *
 *  Created on: 14 Dec 2015
 *      Author: hieu
 */
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "Sentence.h"
#include "../System.h"
#include "../parameters/AllOptions.h"
#include "../legacy/Util2.h"

using namespace std;

namespace Moses2
{

Sentence *Sentence::CreateFromString(MemPool &pool, FactorCollection &vocab,
    const System &system, const std::string &str)
{
  Sentence *ret;

  if (system.options.input.xml_policy) {
    // xml
	ret = CreateFromStringXML(pool, vocab, system, str);
  }
  else {
    // no xml
    //cerr << "PB Sentence" << endl;
    std::vector<std::string> toks = Tokenize(str);

    size_t size = toks.size();
    ret = new (pool.Allocate<Sentence>()) Sentence(pool, size);
    ret->PhraseImplTemplate<Word>::CreateFromString(vocab, system, toks, false);
  }

  //cerr << "REORDERING CONSTRAINTS:" << ret->GetReorderingConstraint() << endl;

  return ret;
}

Sentence *Sentence::CreateFromStringXML(MemPool &pool, FactorCollection &vocab,
    const System &system, const std::string &str)
{
  Sentence *ret;

    vector<XMLOption*> xmlOptions;
    pugi::xml_document doc;

    string str2 = "<xml>" + str + "</xml>";
    pugi::xml_parse_result result = doc.load(str2.c_str(),
                                    pugi::parse_default | pugi::parse_comments);
    pugi::xml_node topNode = doc.child("xml");

    std::vector<std::string> toks;
    XMLParse(pool, system, 0, topNode, toks, xmlOptions);

    // debug
    /*
    cerr << "xmloptions:" << endl;
    for (size_t i = 0; i < xmlOptions.size(); ++i) {
      cerr << xmlOptions[i]->Debug(system) << endl;
    }
	*/

    // create words
    size_t size = toks.size();
    ret = new (pool.Allocate<Sentence>()) Sentence(pool, size);
    ret->PhraseImplTemplate<Word>::CreateFromString(vocab, system, toks, false);

    // xml
    ret->Init(system, size, system.options.reordering.max_distortion);

    ReorderingConstraint &reorderingConstraint = ret->GetReorderingConstraint();

    // set reordering walls, if "-monotone-at-punction" is set
    if (system.options.reordering.monotone_at_punct && ret->GetSize()) {
      reorderingConstraint.SetMonotoneAtPunctuation(*ret);
    }

    // set walls obtained from xml
    for(size_t i=0; i<xmlOptions.size(); i++) {
      const XMLOption *xmlOption = xmlOptions[i];
      if(strcmp(xmlOption->GetNodeName(), "wall") == 0) {
        UTIL_THROW_IF2(xmlOption->startPos >= ret->GetSize(), "wall is beyond the sentence"); // no buggy walls, please
        reorderingConstraint.SetWall(xmlOption->startPos - 1, true);
      }
      else if (strcmp(xmlOption->GetNodeName(), "zone") == 0) {
        reorderingConstraint.SetZone( xmlOption->startPos, xmlOption->startPos + xmlOption->phraseSize -1 );
      }
      else if (strcmp(xmlOption->GetNodeName(), "ne") == 0) {
    	  FactorType placeholderFactor = system.options.input.placeholder_factor;
    	  UTIL_THROW_IF2(placeholderFactor == NOT_FOUND,
    			  "Placeholder XML in input. Must have argument -placeholder-factor [NUM]");
    	  UTIL_THROW_IF2(xmlOption->phraseSize != 1,
    			  "Placeholder must only cover 1 word");

    	  const Factor *factor = vocab.AddFactor(xmlOption->GetEntity(), system, false);
    	  (*ret)[xmlOption->startPos][placeholderFactor] = factor;
      }
      else {
    	// default - forced translation. Add to class variable
    	  ret->AddXMLOption(system, xmlOption);
      }
    }
    reorderingConstraint.FinalizeWalls();

	//cerr << "ret=" << ret->Debug(system) << endl;
    return ret;
}

void Sentence::XMLParse(
	MemPool &pool,
    const System &system,
    size_t depth,
    const pugi::xml_node &parentNode,
    std::vector<std::string> &toks,
    vector<XMLOption*> &xmlOptions)
{  // pugixml
  for (pugi::xml_node childNode = parentNode.first_child(); childNode; childNode = childNode.next_sibling()) {
    string nodeName = childNode.name();
    //cerr << depth << " nodeName=" << nodeName << endl;

    int startPos = toks.size();

    string value = childNode.value();
    if (!value.empty()) {
      //cerr << depth << "childNode text=" << value << endl;
      std::vector<std::string> subPhraseToks = Tokenize(value);
      for (size_t i = 0; i < subPhraseToks.size(); ++i) {
        toks.push_back(subPhraseToks[i]);
      }
    }

    if (!nodeName.empty()) {
      XMLOption *xmlOption = new (pool.Allocate<XMLOption>()) XMLOption(pool, nodeName, startPos);

      pugi::xml_attribute attr;
      attr = childNode.attribute("translation");
      if (!attr.empty()) {
    	  xmlOption->SetTranslation(pool, attr.as_string());
      }

      attr = childNode.attribute("entity");
      if (!attr.empty()) {
    	  xmlOption->SetEntity(pool, attr.as_string());
      }

      attr = childNode.attribute("prob");
      if (!attr.empty()) {
    	  xmlOption->prob = attr.as_float();
      }

      xmlOptions.push_back(xmlOption);

      // recursively call this function. For proper recursive trees
      XMLParse(pool, system, depth + 1, childNode, toks, xmlOptions);

      size_t endPos = toks.size();
      xmlOption->phraseSize = endPos - startPos;

      /*
      cerr << "xmlOptions=";
      xmlOption->Debug(cerr, system);
      cerr << endl;
      */
    }

  }
}

} /* namespace Moses2 */

