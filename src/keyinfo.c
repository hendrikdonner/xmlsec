/*
 * XML Security Library (http://www.aleksey.com/xmlsec).
 *
 *
 * This is free software; see Copyright file in the source
 * distribution for preciese wording.
 *
 * Copyright (C) 2002-2022 Aleksey Sanin <aleksey@aleksey.com>. All Rights Reserved.
 */
/**
 * SECTION:keyinfo
 * @Short_description: &lt;dsig:KeyInfo/&gt; node parser functions.
 * @Stability: Stable
 *
 *
 * [KeyInfo](https://www.w3.org/TR/xmldsig-core/#sec-KeyInfo) is an
 * optional element that enables the recipient(s) to obtain
 * the key needed to validate the signature.  KeyInfo may contain keys,
 * names, certificates and other public key management information, such as
 * in-band key distribution or key agreement data.
 *
 * Schema Definition:
 *
 * |[<!-- language="XML" -->
 *  <element name="KeyInfo" type="ds:KeyInfoType"/>
 *  <complexType name="KeyInfoType" mixed="true">
 *    <choice maxOccurs="unbounded">
 *       <element ref="ds:KeyName"/>
 *       <element ref="ds:KeyValue"/>
 *       <element ref="ds:RetrievalMethod"/>
 *       <element ref="ds:X509Data"/>
 *       <element ref="ds:PGPData"/>
 *       <element ref="ds:SPKIData"/>
 *       <element ref="ds:MgmtData"/>
 *       <any processContents="lax" namespace="##other"/>
 *       <!-- (1,1) elements from (0,unbounded) namespaces -->
 *    </choice>
 *    <attribute name="Id" type="ID" use="optional"/>
 *  </complexType>
 * ]|
 *
 * DTD:
 *
 * |[<!-- language="XML" -->
 * <!ELEMENT KeyInfo (#PCDATA|KeyName|KeyValue|RetrievalMethod|
 *                    X509Data|PGPData|SPKIData|MgmtData %KeyInfo.ANY;)* >
 * <!ATTLIST KeyInfo  Id  ID   #IMPLIED >
 * ]|
 */

#include "globals.h"

#include <stdlib.h>
#include <string.h>

#include <libxml/tree.h>

#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/base64.h>
#include <xmlsec/keys.h>
#include <xmlsec/keysmngr.h>
#include <xmlsec/transforms.h>
#include <xmlsec/xmlenc.h>
#include <xmlsec/keyinfo.h>
#include <xmlsec/errors.h>

#include "cast_helpers.h"

/**************************************************************************
 *
 * High-level functions
 *
 *************************************************************************/
/**
 * xmlSecKeyInfoNodeRead:
 * @keyInfoNode:        the pointer to &lt;dsig:KeyInfo/&gt; node.
 * @key:                the pointer to result key object.
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 *
 * Parses the &lt;dsig:KeyInfo/&gt; element @keyInfoNode, extracts the key data
 * and stores into @key.
 *
 * Returns: 0 on success or -1 if an error occurs.
 */
int
xmlSecKeyInfoNodeRead(xmlNodePtr keyInfoNode, xmlSecKeyPtr key, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    const xmlChar* nodeName;
    const xmlChar* nodeNs;
    xmlSecKeyDataId dataId;
    xmlNodePtr cur;
    int ret;

    xmlSecAssert2(keyInfoNode != NULL, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    for(cur = xmlSecGetNextElementNode(keyInfoNode->children);
        (cur != NULL) &&
        (((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_DONT_STOP_ON_KEY_FOUND) != 0) ||
         (xmlSecKeyIsValid(key) == 0) ||
         (xmlSecKeyMatch(key, NULL, &(keyInfoCtx->keyReq)) == 0));
        cur = xmlSecGetNextElementNode(cur->next)) {

        /* find data id */
        nodeName = cur->name;
        nodeNs = xmlSecGetNodeNsHref(cur);

        /* use global list only if we don't have a local one */
        if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
            dataId = xmlSecKeyDataIdListFindByNode(&(keyInfoCtx->enabledKeyData),
                            nodeName, nodeNs, xmlSecKeyDataUsageKeyInfoNodeRead);
        } else {
            dataId = xmlSecKeyDataIdListFindByNode(xmlSecKeyDataIdsGet(),
                            nodeName, nodeNs, xmlSecKeyDataUsageKeyInfoNodeRead);
        }
        if(dataId != xmlSecKeyDataIdUnknown) {
            /* read data node */
            ret = xmlSecKeyDataXmlRead(dataId, key, cur, keyInfoCtx);
            if(ret < 0) {
                xmlSecInternalError2("xmlSecKeyDataXmlRead",
                                     xmlSecKeyDataKlassGetName(dataId),
                                     "node=%s",
                                     xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
                return(-1);
            }
        } else if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_STOP_ON_UNKNOWN_CHILD) != 0) {
            /* there is a laxi schema validation but application may
             * desire to disable unknown nodes*/
            xmlSecUnexpectedNodeError(cur, NULL);
            return(-1);
        }
    }

    return(0);
}

/**
 * xmlSecKeyInfoNodeWrite:
 * @keyInfoNode:        the pointer to &lt;dsig:KeyInfo/&gt; node.
 * @key:                the pointer to key object.
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 *
 * Writes the @key into the &lt;dsig:KeyInfo/&gt; element template @keyInfoNode.
 *
 * Returns: 0 on success or -1 if an error occurs.
 */
int
xmlSecKeyInfoNodeWrite(xmlNodePtr keyInfoNode, xmlSecKeyPtr key, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    const xmlChar* nodeName;
    const xmlChar* nodeNs;
    xmlSecKeyDataId dataId;
    xmlNodePtr cur;
    int ret;

    xmlSecAssert2(keyInfoNode != NULL, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    for(cur = xmlSecGetNextElementNode(keyInfoNode->children);
        cur != NULL;
        cur = xmlSecGetNextElementNode(cur->next)) {

        /* find data id */
        nodeName = cur->name;
        nodeNs = xmlSecGetNodeNsHref(cur);

        /* use global list only if we don't have a local one */
        if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
                dataId = xmlSecKeyDataIdListFindByNode(&(keyInfoCtx->enabledKeyData),
                            nodeName, nodeNs,
                            xmlSecKeyDataUsageKeyInfoNodeWrite);
        } else {
                dataId = xmlSecKeyDataIdListFindByNode(xmlSecKeyDataIdsGet(),
                            nodeName, nodeNs,
                            xmlSecKeyDataUsageKeyInfoNodeWrite);
        }
        if(dataId != xmlSecKeyDataIdUnknown) {
            ret = xmlSecKeyDataXmlWrite(dataId, key, cur, keyInfoCtx);
            if(ret < 0) {
                xmlSecInternalError2("xmlSecKeyDataXmlWrite",
                                     xmlSecKeyDataKlassGetName(dataId),
                                     "node=%s",
                                     xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
                return(-1);
            }
        } else if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_STOP_ON_UNKNOWN_CHILD) != 0) {
            /* laxi schema validation but application can disable it*/
            xmlSecUnexpectedNodeError(cur, NULL);
            return(-1);
        }
    }

    return(0);
}

/**************************************************************************
 *
 * KeyInfo context
 *
 *************************************************************************/
/**
 * xmlSecKeyInfoCtxCreate:
 * @keysMngr:           the pointer to keys manager (may be NULL).
 *
 * Allocates and initializes &lt;dsig:KeyInfo/&gt; element processing context.
 * Caller is responsible for freeing it by calling #xmlSecKeyInfoCtxDestroy
 * function.
 *
 * Returns: pointer to newly allocated object or NULL if an error occurs.
 */
xmlSecKeyInfoCtxPtr
xmlSecKeyInfoCtxCreate(xmlSecKeysMngrPtr keysMngr) {
    xmlSecKeyInfoCtxPtr keyInfoCtx;
    int ret;

    /* Allocate a new xmlSecKeyInfoCtx and fill the fields. */
    keyInfoCtx = (xmlSecKeyInfoCtxPtr)xmlMalloc(sizeof(xmlSecKeyInfoCtx));
    if(keyInfoCtx == NULL) {
        xmlSecMallocError(sizeof(xmlSecKeyInfoCtx), NULL);
        return(NULL);
    }

    ret = xmlSecKeyInfoCtxInitialize(keyInfoCtx, keysMngr);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxInitialize", NULL);
        xmlSecKeyInfoCtxDestroy(keyInfoCtx);
        return(NULL);
    }

    return(keyInfoCtx);
}

/**
 * xmlSecKeyInfoCtxDestroy:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 *
 * Destroys @keyInfoCtx object created with #xmlSecKeyInfoCtxCreate function.
 */
void
xmlSecKeyInfoCtxDestroy(xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecAssert(keyInfoCtx != NULL);

    xmlSecKeyInfoCtxFinalize(keyInfoCtx);
    xmlFree(keyInfoCtx);
}

/**
 * xmlSecKeyInfoCtxInitialize:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 * @keysMngr:           the pointer to keys manager (may be NULL).
 *
 * Initializes &lt;dsig:KeyInfo/&gt; element processing context. Caller is
 * responsible for cleaning it up by #xmlSecKeyInfoCtxFinalize function.
 *
 * Returns: 0 on success and a negative value if an error occurs.
 */
int
xmlSecKeyInfoCtxInitialize(xmlSecKeyInfoCtxPtr keyInfoCtx, xmlSecKeysMngrPtr keysMngr) {
    int ret;

    xmlSecAssert2(keyInfoCtx != NULL, -1);

    memset(keyInfoCtx, 0, sizeof(xmlSecKeyInfoCtx));
    keyInfoCtx->keysMngr = keysMngr;
    keyInfoCtx->base64LineSize = xmlSecBase64GetDefaultLineSize();
    ret = xmlSecPtrListInitialize(&(keyInfoCtx->enabledKeyData), xmlSecKeyDataIdListId);
    if(ret < 0) {
        xmlSecInternalError("xmlSecPtrListInitialize", NULL);
        return(-1);
    }

    keyInfoCtx->maxRetrievalMethodLevel = 1;
    ret = xmlSecTransformCtxInitialize(&(keyInfoCtx->retrievalMethodCtx));
    if(ret < 0) {
        xmlSecInternalError("xmlSecTransformCtxInitialize(retrievalMethodCtx)", NULL);
        return(-1);
    }

    keyInfoCtx->maxKeyInfoReferenceLevel = 1;
    ret = xmlSecTransformCtxInitialize(&(keyInfoCtx->keyInfoReferenceCtx));
    if(ret < 0) {
        xmlSecInternalError("xmlSecTransformCtxInitialize(keyInfoReferenceCtx)", NULL);
        return(-1);
    }

#ifndef XMLSEC_NO_XMLENC
    keyInfoCtx->maxEncryptedKeyLevel = 1;
#endif /* XMLSEC_NO_XMLENC */

#ifndef XMLSEC_NO_X509
    keyInfoCtx->certsVerificationDepth= 9;
#endif /* XMLSEC_NO_X509 */

    ret = xmlSecKeyReqInitialize(&(keyInfoCtx->keyReq));
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyReqInitialize", NULL);
        return(-1);
    }

    return(0);
}

/**
 * xmlSecKeyInfoCtxFinalize:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 *
 * Cleans up the @keyInfoCtx initialized with #xmlSecKeyInfoCtxInitialize
 * function.
 */
void
xmlSecKeyInfoCtxFinalize(xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecAssert(keyInfoCtx != NULL);

    xmlSecPtrListFinalize(&(keyInfoCtx->enabledKeyData));
    xmlSecTransformCtxFinalize(&(keyInfoCtx->retrievalMethodCtx));
    xmlSecTransformCtxFinalize(&(keyInfoCtx->keyInfoReferenceCtx));
    xmlSecKeyReqFinalize(&(keyInfoCtx->keyReq));

#ifndef XMLSEC_NO_XMLENC
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxDestroy(keyInfoCtx->encCtx);
    }
#endif /* XMLSEC_NO_XMLENC */

    memset(keyInfoCtx, 0, sizeof(xmlSecKeyInfoCtx));
}

/**
 * xmlSecKeyInfoCtxReset:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 *
 * Resets the @keyInfoCtx state. User settings are not changed.
 */
void
xmlSecKeyInfoCtxReset(xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecAssert(keyInfoCtx != NULL);

    xmlSecTransformCtxReset(&(keyInfoCtx->retrievalMethodCtx));
    keyInfoCtx->curRetrievalMethodLevel = 0;

    xmlSecTransformCtxReset(&(keyInfoCtx->keyInfoReferenceCtx));
    keyInfoCtx->curKeyInfoReferenceLevel = 0;

    keyInfoCtx->curEncryptedKeyLevel = 0;

    keyInfoCtx->operation = xmlSecTransformOperationNone;

#ifndef XMLSEC_NO_XMLENC
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxReset(keyInfoCtx->encCtx);
    }
#endif /* XMLSEC_NO_XMLENC */

    xmlSecKeyReqReset(&(keyInfoCtx->keyReq));
}

/**
 * xmlSecKeyInfoCtxCreateEncCtx:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 *
 * Creates encryption context form processing &lt;enc:EncryptedKey/&gt; child
 * of &lt;dsig:KeyInfo/&gt; element.
 *
 * Returns: 0 on success and a negative value if an error occurs.
 */
int
xmlSecKeyInfoCtxCreateEncCtx(xmlSecKeyInfoCtxPtr keyInfoCtx) {
#ifndef XMLSEC_NO_XMLENC
    xmlSecEncCtxPtr tmp;
    int ret;

    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->encCtx == NULL, -1);

    /* we have to use tmp variable to avoid a recursive loop */
    tmp = xmlSecEncCtxCreate(keyInfoCtx->keysMngr);
    if(tmp == NULL) {
        xmlSecInternalError("xmlSecEncCtxCreate", NULL);
        return(-1);
    }
    tmp->mode = xmlEncCtxModeEncryptedKey;

    /* copy user preferences from our current ctx */
    switch(keyInfoCtx->mode) {
        case xmlSecKeyInfoModeRead:
            ret = xmlSecKeyInfoCtxCopyUserPref(&(tmp->keyInfoReadCtx), keyInfoCtx);
            if(ret < 0) {
                xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref", NULL);
                xmlSecEncCtxDestroy(tmp);
                return(-1);
            }
            break;
        case xmlSecKeyInfoModeWrite:
            ret = xmlSecKeyInfoCtxCopyUserPref(&(tmp->keyInfoWriteCtx), keyInfoCtx);
            if(ret < 0) {
                xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref", NULL);
                xmlSecEncCtxDestroy(tmp);
                return(-1);
            }
            break;
    }
    keyInfoCtx->encCtx = tmp;
    tmp->keyInfoReadCtx.operation = keyInfoCtx->operation;
    tmp->keyInfoWriteCtx.operation = keyInfoCtx->operation;

    return(0);
#else /* XMLSEC_NO_XMLENC */

    xmlSecOtherError(XMLSEC_ERRORS_R_DISABLED, NULL, "xml encryption");
    return(-1);
#endif /* XMLSEC_NO_XMLENC */
}

/**
 * xmlSecKeyInfoCtxCopyUserPref:
 * @dst:                the pointer to destination context object.
 * @src:                the pointer to source context object.
 *
 * Copies user preferences from @src context to @dst context.
 *
 * Returns: 0 on success and a negative value if an error occurs.
 */
int
xmlSecKeyInfoCtxCopyUserPref(xmlSecKeyInfoCtxPtr dst, xmlSecKeyInfoCtxPtr src) {
    int ret;

    xmlSecAssert2(dst != NULL, -1);
    xmlSecAssert2(src != NULL, -1);

    dst->userData       = src->userData;
    dst->flags          = src->flags;
    dst->flags2         = src->flags2;
    dst->keysMngr       = src->keysMngr;
    dst->base64LineSize = src->base64LineSize;

    ret = xmlSecPtrListCopy(&(dst->enabledKeyData), &(src->enabledKeyData));
    if(ret < 0) {
        xmlSecInternalError("xmlSecPtrListCopy(enabledKeyData)", NULL);
        return(-1);
    }

    /* &lt;dsig:RetrievalMethod/&gt; */
    dst->maxRetrievalMethodLevel= src->maxRetrievalMethodLevel;
    ret = xmlSecTransformCtxCopyUserPref(&(dst->retrievalMethodCtx), &(src->retrievalMethodCtx));
    if(ret < 0) {
        xmlSecInternalError("xmlSecTransformCtxCopyUserPref(enabledKeyData)", NULL);
        return(-1);
    }

    /* &lt;dsig:KeyInfoReference/&gt; */
    dst->maxKeyInfoReferenceLevel = src->maxKeyInfoReferenceLevel;
    ret = xmlSecTransformCtxCopyUserPref(&(dst->keyInfoReferenceCtx), &(src->keyInfoReferenceCtx));
    if(ret < 0) {
        xmlSecInternalError("xmlSecTransformCtxCopyUserPref(enabledKeyData)", NULL);
        return(-1);
    }

    /* <enc:EncryptedContext /> */
#ifndef XMLSEC_NO_XMLENC
    if((src->encCtx != NULL) && (dst->encCtx != NULL)) {
        dst->encCtx->mode = xmlEncCtxModeEncryptedKey;
        ret = xmlSecEncCtxCopyUserPref(dst->encCtx, src->encCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecEncCtxCopyUserPref", NULL);
            return(-1);
        }
    }
    dst->maxEncryptedKeyLevel   = src->maxEncryptedKeyLevel;
#endif /* XMLSEC_NO_XMLENC */

    /* <dsig:X509Data /> */
#ifndef XMLSEC_NO_X509
    dst->certsVerificationTime  = src->certsVerificationTime;
    dst->certsVerificationDepth = src->certsVerificationDepth;
#endif /* XMLSEC_NO_X509 */

    return(0);
}

/**
 * xmlSecKeyInfoCtxDebugDump:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 * @output:             the output file pointer.
 *
 * Prints user settings and current context state to @output.
 */
void
xmlSecKeyInfoCtxDebugDump(xmlSecKeyInfoCtxPtr keyInfoCtx, FILE* output) {
    xmlSecAssert(keyInfoCtx != NULL);
    xmlSecAssert(output != NULL);

    switch(keyInfoCtx->mode) {
        case xmlSecKeyInfoModeRead:
            fprintf(output, "= KEY INFO READ CONTEXT\n");
            break;
        case xmlSecKeyInfoModeWrite:
            fprintf(output, "= KEY INFO WRITE CONTEXT\n");
            break;
    }

    fprintf(output, "== flags: 0x%08x\n", keyInfoCtx->flags);
    fprintf(output, "== flags2: 0x%08x\n", keyInfoCtx->flags2);
    if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
        fprintf(output, "== enabled key data: ");
        xmlSecKeyDataIdListDebugDump(&(keyInfoCtx->enabledKeyData), output);
    } else {
        fprintf(output, "== enabled key data: all\n");
    }

    fprintf(output, "== RetrievalMethod level (cur/max): %d/%d\n",
            keyInfoCtx->curRetrievalMethodLevel, keyInfoCtx->maxRetrievalMethodLevel);
    xmlSecTransformCtxDebugDump(&(keyInfoCtx->retrievalMethodCtx), output);

    fprintf(output, "== KeyInfoReference level (cur/max): %d/%d\n",
            keyInfoCtx->curKeyInfoReferenceLevel, keyInfoCtx->maxKeyInfoReferenceLevel);
    xmlSecTransformCtxDebugDump(&(keyInfoCtx->keyInfoReferenceCtx), output);

#ifndef XMLSEC_NO_XMLENC
    fprintf(output, "== EncryptedKey level (cur/max): %d/%d\n",
            keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxDebugDump(keyInfoCtx->encCtx, output);
    }
#endif /* XMLSEC_NO_XMLENC */

    xmlSecKeyReqDebugDump(&(keyInfoCtx->keyReq), output);
}

/**
 * xmlSecKeyInfoCtxDebugXmlDump:
 * @keyInfoCtx:         the pointer to &lt;dsig:KeyInfo/&gt; element processing context.
 * @output:             the output file pointer.
 *
 * Prints user settings and current context state in XML format to @output.
 */
void
xmlSecKeyInfoCtxDebugXmlDump(xmlSecKeyInfoCtxPtr keyInfoCtx, FILE* output) {
    xmlSecAssert(keyInfoCtx != NULL);
    xmlSecAssert(output != NULL);

    switch(keyInfoCtx->mode) {
        case xmlSecKeyInfoModeRead:
            fprintf(output, "<KeyInfoReadContext>\n");
            break;
        case xmlSecKeyInfoModeWrite:
            fprintf(output, "<KeyInfoWriteContext>\n");
            break;
    }

    fprintf(output, "<Flags>%08x</Flags>\n", keyInfoCtx->flags);
    fprintf(output, "<Flags2>%08x</Flags2>\n", keyInfoCtx->flags2);
    if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
        fprintf(output, "<EnabledKeyData>\n");
        xmlSecKeyDataIdListDebugXmlDump(&(keyInfoCtx->enabledKeyData), output);
        fprintf(output, "</EnabledKeyData>\n");
    } else {
        fprintf(output, "<EnabledKeyData>all</EnabledKeyData>\n");
    }

    fprintf(output, "<RetrievalMethodLevel cur=\"%d\" max=\"%d\" />\n",
        keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
    xmlSecTransformCtxDebugXmlDump(&(keyInfoCtx->retrievalMethodCtx), output);

    fprintf(output, "<KeyInfoReferenceLevel cur=\"%d\" max=\"%d\" />\n",
        keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
    xmlSecTransformCtxDebugXmlDump(&(keyInfoCtx->keyInfoReferenceCtx), output);

#ifndef XMLSEC_NO_XMLENC
    fprintf(output, "<EncryptedKeyLevel cur=\"%d\" max=\"%d\" />\n",
        keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxDebugXmlDump(keyInfoCtx->encCtx, output);
    }
#endif /* XMLSEC_NO_XMLENC */

    xmlSecKeyReqDebugXmlDump(&(keyInfoCtx->keyReq), output);
    switch(keyInfoCtx->mode) {
        case xmlSecKeyInfoModeRead:
            fprintf(output, "</KeyInfoReadContext>\n");
            break;
        case xmlSecKeyInfoModeWrite:
            fprintf(output, "</KeyInfoWriteContext>\n");
            break;
    }
}

/**************************************************************************
 *
 * &lt;dsig:KeyName/&gt; processing
 *
 *************************************************************************/
static int                      xmlSecKeyDataNameXmlRead        (xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);
static int                      xmlSecKeyDataNameXmlWrite       (xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);

static xmlSecKeyDataKlass xmlSecKeyDataNameKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameKeyName,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml, /* xmlSecKeyDataUsage usage; */
    NULL,                                       /* const xmlChar* href; */
    xmlSecNodeKeyName,                          /* const xmlChar* dataNodeName; */
    xmlSecDSigNs,                               /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataNameXmlRead,                   /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataNameXmlWrite,                  /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

/**
 * xmlSecKeyDataNameGetKlass:
 *
 * The &lt;dsig:KeyName/&gt; element key data klass
 * (http://www.w3.org/TR/xmldsig-core/#sec-KeyName):
 *
 * The KeyName element contains a string value (in which white space is
 * significant) which may be used by the signer to communicate a key
 * identifier to the recipient. Typically, KeyName contains an identifier
 * related to the key pair used to sign the message, but it may contain
 * other protocol-related information that indirectly identifies a key pair.
 * (Common uses of KeyName include simple string names for keys, a key index,
 * a distinguished name (DN), an email address, etc.)
 *
 * Returns: the &lt;dsig:KeyName/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataNameGetKlass(void) {
    return(&xmlSecKeyDataNameKlass);
}

static int
xmlSecKeyDataNameXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlChar* newName;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataNameId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    /* read key name */
    newName = xmlSecGetNodeContentAndTrim(node);
    if(newName == NULL) {
        xmlSecInvalidNodeContentError(node, xmlSecKeyDataKlassGetName(id), "empty");
        return(-1);
    }

    /* try to find key in the manager */
    if((xmlSecKeyGetValue(key) == NULL) && (keyInfoCtx->keysMngr != NULL)) {
        xmlSecKeyPtr tmpKey;

        tmpKey = xmlSecKeysMngrFindKey(keyInfoCtx->keysMngr, newName, keyInfoCtx);
        if(tmpKey != NULL) {
            /* erase any current information in the key */
            xmlSecKeyEmpty(key);

            /* TODO: since we will destroy tmpKey anyway, we can easily
             * just re-assign key data values. It'll save use some memory
             * malloc/free
             */

            /* and copy what we've found */
            ret = xmlSecKeyCopy(key, tmpKey);
            if(ret < 0) {
                xmlSecInternalError("xmlSecKeyCopy",
                                    xmlSecKeyDataKlassGetName(id));
                xmlSecKeyDestroy(tmpKey);
                xmlFree(newName);
                return(-1);
            }
            xmlSecKeyDestroy(tmpKey);

            /* and set the key name */
            ret = xmlSecKeySetName(key, newName);
            if(ret < 0) {
                xmlSecInternalError("xmlSecKeySetName",
                                    xmlSecKeyDataKlassGetName(id));
                xmlFree(newName);
                return(-1);
            }
        }
        /* TODO: record the key names we tried */
    } else {
        const xmlChar* oldName;

        /* if we already have a keyname, make sure that it matches or set it */
        oldName = xmlSecKeyGetName(key);
        if(oldName != NULL) {
            if(!xmlStrEqual(oldName, newName)) {
                xmlSecOtherError(XMLSEC_ERRORS_R_INVALID_KEY_DATA,
                                 xmlSecKeyDataKlassGetName(id),
                                 "key name is already specified");
                xmlFree(newName);
                return(-1);
            }
        } else {
            ret = xmlSecKeySetName(key, newName);
            if(ret < 0) {
                xmlSecInternalError("xmlSecKeySetName",
                                    xmlSecKeyDataKlassGetName(id));
                xmlFree(newName);
                return(-1);
            }
        }
    }

    /* done */
    xmlFree(newName);
    return(0);
}

static int
xmlSecKeyDataNameXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    const xmlChar* name;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataNameId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    name = xmlSecKeyGetName(key);
    if(name == NULL) {
        return(8);
    }

    if(!xmlSecIsEmptyNode(node)) {
        return(0);
    }

    ret = xmlSecNodeEncodeAndSetContent(node, name);
    if(ret < 0) {
        xmlSecInternalError("xmlSecNodeEncodeAndSetContent", NULL);
        return(-1);
    }

    /* done */
    return(0);
}

/**************************************************************************
 *
 * &lt;dsig:KeyValue/&gt; processing
 *
 *************************************************************************/
static int                      xmlSecKeyDataValueXmlRead       (xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);
static int                      xmlSecKeyDataValueXmlWrite      (xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);

static xmlSecKeyDataKlass xmlSecKeyDataValueKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameKeyValue,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml, /* xmlSecKeyDataUsage usage; */
    NULL,                                       /* const xmlChar* href; */
    xmlSecNodeKeyValue,                         /* const xmlChar* dataNodeName; */
    xmlSecDSigNs,                               /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataValueXmlRead,                  /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataValueXmlWrite,                 /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

/**
 * xmlSecKeyDataValueGetKlass:
 *
 * The &lt;dsig:KeyValue/&gt; element key data klass
 * (http://www.w3.org/TR/xmldsig-core/#sec-KeyValue):
 *
 * The KeyValue element contains a single public key that may be useful in
 * validating the signature.
 *
 * Returns: the &lt;dsig:KeyValue/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataValueGetKlass(void) {
    return(&xmlSecKeyDataValueKlass);
}

static int
xmlSecKeyDataValueXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    const xmlChar* nodeName;
    const xmlChar* nodeNs;
    xmlSecKeyDataId dataId;
    xmlNodePtr cur;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataValueId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    cur = xmlSecGetNextElementNode(node->children);
    if(cur == NULL) {
        /* just an empty node */
        return(0);
    }

    /* find data id */
    nodeName = cur->name;
    nodeNs = xmlSecGetNodeNsHref(cur);

    /* use global list only if we don't have a local one */
    if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
        dataId = xmlSecKeyDataIdListFindByNode(&(keyInfoCtx->enabledKeyData),
                            nodeName, nodeNs, xmlSecKeyDataUsageKeyValueNodeRead);
    } else {
        dataId = xmlSecKeyDataIdListFindByNode(xmlSecKeyDataIdsGet(),
                            nodeName, nodeNs, xmlSecKeyDataUsageKeyValueNodeRead);
    }
    if(dataId != xmlSecKeyDataIdUnknown) {
        /* read data node */
        ret = xmlSecKeyDataXmlRead(dataId, key, cur, keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError2("xmlSecKeyDataXmlRead",
                                 xmlSecKeyDataKlassGetName(id),
                                 "node=%s",
                                 xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
            return(-1);
        }
    } else if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_KEYVALUE_STOP_ON_UNKNOWN_CHILD) != 0) {
        /* laxi schema validation but application can disable it */
        xmlSecUnexpectedNodeError(cur, xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    /* &lt;dsig:KeyValue/&gt; might have only one node */
    cur = xmlSecGetNextElementNode(cur->next);
    if(cur != NULL) {
        xmlSecUnexpectedNodeError(cur, xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    return(0);
}

static int
xmlSecKeyDataValueXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    const xmlChar* nodeName;
    const xmlChar* nodeNs;
    xmlNodePtr cur;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataValueId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    if(!xmlSecKeyDataIsValid(key->value) ||
       !xmlSecKeyDataCheckUsage(key->value, xmlSecKeyDataUsageKeyValueNodeWrite)){
        /* nothing to write */
        return(0);
    }
    if((xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) &&
        (xmlSecKeyDataIdListFind(&(keyInfoCtx->enabledKeyData), id) != 1)) {

        /* we are not enabled to write out key data with this id */
        return(0);
    }
    if(xmlSecKeyReqMatchKey(&(keyInfoCtx->keyReq), key) != 1) {
        /* we are not allowed to write out this key */
        return(0);
    }

    nodeName = key->value->id->dataNodeName;
    nodeNs = key->value->id->dataNodeNs;
    xmlSecAssert2(nodeName != NULL, -1);

    /* remove all existing key value */
    xmlNodeSetContent(node, NULL);

    /* create key node */
    cur = xmlSecAddChild(node, nodeName, nodeNs);
    if(cur == NULL) {
        xmlSecInternalError2("xmlSecAddChild",
                             xmlSecKeyDataKlassGetName(id),
                             "node=%s",
                             xmlSecErrorsSafeString(xmlSecNodeGetName(node)));
        return(-1);
    }

    ret = xmlSecKeyDataXmlWrite(key->value->id, key, cur, keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError2("xmlSecKeyDataXmlWrite",
                             xmlSecKeyDataKlassGetName(id),
                             "node=%s",
                             xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
        return(-1);
    }

    return(0);
}

/**************************************************************************
 *
 * &lt;dsig:RetrievalMethod/&gt; processing
 *
 *************************************************************************/
static int                      xmlSecKeyDataRetrievalMethodXmlRead(xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);
static int                      xmlSecKeyDataRetrievalMethodXmlWrite(xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);



static xmlSecKeyDataKlass xmlSecKeyDataRetrievalMethodKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameRetrievalMethod,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml, /* xmlSecKeyDataUsage usage; */
    NULL,                                       /* const xmlChar* href; */
    xmlSecNodeRetrievalMethod,                  /* const xmlChar* dataNodeName; */
    xmlSecDSigNs,                               /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataRetrievalMethodXmlRead,        /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataRetrievalMethodXmlWrite,       /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

static int                      xmlSecKeyDataRetrievalMethodReadXmlResult(xmlSecKeyDataId typeId,
                                                                 xmlSecKeyPtr key,
                                                                 const xmlChar* buffer,
                                                                 xmlSecSize bufferSize,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);

/**
 * xmlSecKeyDataRetrievalMethodGetKlass:
 *
 * The &lt;dsig:RetrievalMethod/&gt; element key data klass
 * (http://www.w3.org/TR/xmldsig-core/#sec-RetrievalMethod):
 * A RetrievalMethod element within KeyInfo is used to convey a reference to
 * KeyInfo information that is stored at another location. For example,
 * several signatures in a document might use a key verified by an X.509v3
 * certificate chain appearing once in the document or remotely outside the
 * document; each signature's KeyInfo can reference this chain using a single
 * RetrievalMethod element instead of including the entire chain with a
 * sequence of X509Certificate elements.
 *
 * RetrievalMethod uses the same syntax and dereferencing behavior as
 * Reference's URI and The Reference Processing Model.
 *
 * Returns: the &lt;dsig:RetrievalMethod/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataRetrievalMethodGetKlass(void) {
    return(&xmlSecKeyDataRetrievalMethodKlass);
}

static int
xmlSecKeyDataRetrievalMethodXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyDataId dataId = xmlSecKeyDataIdUnknown;
    xmlChar *retrType = NULL;
    xmlChar *uri = NULL;
    xmlNodePtr cur;
    int res = -1;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataRetrievalMethodId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(node->doc != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    /* check retrieval level */
    if(keyInfoCtx->curRetrievalMethodLevel >= keyInfoCtx->maxRetrievalMethodLevel) {
        xmlSecOtherError3(XMLSEC_ERRORS_R_MAX_RETRIEVALS_LEVEL, xmlSecKeyDataKlassGetName(id),
            "cur=%d;max=%d",keyInfoCtx->curRetrievalMethodLevel, keyInfoCtx->maxRetrievalMethodLevel);
        goto done;
    }
    ++keyInfoCtx->curRetrievalMethodLevel;

    retrType = xmlGetProp(node, xmlSecAttrType);
    if(retrType != NULL) {
        /* use global list only if we don't have a local one */
        if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
            dataId = xmlSecKeyDataIdListFindByHref(&(keyInfoCtx->enabledKeyData),
                            retrType, xmlSecKeyDataUsageRetrievalMethodNode);
        } else {
            dataId = xmlSecKeyDataIdListFindByHref(xmlSecKeyDataIdsGet(),
                            retrType, xmlSecKeyDataUsageRetrievalMethodNode);
        }
    }

    /* laxi schema validation but application can disable it */
    if(dataId == xmlSecKeyDataIdUnknown) {
        if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_RETRMETHOD_STOP_ON_UNKNOWN_HREF) != 0) {
            xmlSecInvalidNodeAttributeError(node, xmlSecAttrType, xmlSecKeyDataKlassGetName(id),
                "retrieval type is unknown");
            goto done;
        }

        res = 0;
        goto done;
    }

    /* destroy prev retrieval method context */
    xmlSecTransformCtxReset(&(keyInfoCtx->retrievalMethodCtx));

    /* set start URI and check that it is enabled */
    uri = xmlGetProp(node, xmlSecAttrURI);
    ret = xmlSecTransformCtxSetUri(&(keyInfoCtx->retrievalMethodCtx), uri, node);
    if(ret < 0) {
        xmlSecInternalError2("xmlSecTransformCtxSetUri",
                             xmlSecKeyDataKlassGetName(id),
                             "uri=%s", xmlSecErrorsSafeString(uri));
        goto done;
    }

    /* the only one node is optional Transforms node */
    cur = xmlSecGetNextElementNode(node->children);
    if((cur != NULL) && (xmlSecCheckNodeName(cur, xmlSecNodeTransforms, xmlSecDSigNs))) {
        ret = xmlSecTransformCtxNodesListRead(&(keyInfoCtx->retrievalMethodCtx),
                                            cur, xmlSecTransformUsageDSigTransform);
        if(ret < 0) {
            xmlSecInternalError2("xmlSecTransformCtxNodesListRead",
                                 xmlSecKeyDataKlassGetName(id),
                                 "node=%s",
                                 xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
            goto done;
        }
        cur = xmlSecGetNextElementNode(cur->next);
    }

    if(cur != NULL) {
        xmlSecUnexpectedNodeError(cur, xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    /* finally get transforms results */
    ret = xmlSecTransformCtxExecute(&(keyInfoCtx->retrievalMethodCtx), node->doc);
    if((ret < 0) ||
       (keyInfoCtx->retrievalMethodCtx.result == NULL) ||
       (xmlSecBufferGetData(keyInfoCtx->retrievalMethodCtx.result) == NULL)) {

        xmlSecInternalError("xmlSecTransformCtxExecute",
                            xmlSecKeyDataKlassGetName(id));
        goto done;
    }


    /* assume that the data is in XML if we could not find id */
    if((dataId == xmlSecKeyDataIdUnknown) ||
       ((dataId->usage & xmlSecKeyDataUsageRetrievalMethodNodeXml) != 0)) {

        ret = xmlSecKeyDataRetrievalMethodReadXmlResult(dataId, key,
                    xmlSecBufferGetData(keyInfoCtx->retrievalMethodCtx.result),
                    xmlSecBufferGetSize(keyInfoCtx->retrievalMethodCtx.result),
                    keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyDataRetrievalMethodReadXmlResult",
                                xmlSecKeyDataKlassGetName(id));
            goto done;
        }
    } else {
        ret = xmlSecKeyDataBinRead(dataId, key,
                    xmlSecBufferGetData(keyInfoCtx->retrievalMethodCtx.result),
                    xmlSecBufferGetSize(keyInfoCtx->retrievalMethodCtx.result),
                    keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyDataBinRead",
                                xmlSecKeyDataKlassGetName(id));
            goto done;
        }
    }
    --keyInfoCtx->curRetrievalMethodLevel;

    res = 0;
done:
    if(uri != NULL) {
        xmlFree(uri);
    }
    if(retrType != NULL) {
        xmlFree(retrType);
    }
    return(res);
}

static int
xmlSecKeyDataRetrievalMethodXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecAssert2(id == xmlSecKeyDataRetrievalMethodId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    /* just do nothing */
    return(0);
}

static int
xmlSecKeyDataRetrievalMethodReadXmlResult(xmlSecKeyDataId typeId, xmlSecKeyPtr key,
                                          const xmlChar* buffer, xmlSecSize bufferSize,
                                          xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlDocPtr doc;
    xmlNodePtr cur;
    const xmlChar* nodeName;
    const xmlChar* nodeNs;
    xmlSecKeyDataId dataId;
    int bufferLen;
    int ret;

    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(buffer != NULL, -1);
    xmlSecAssert2(bufferSize > 0, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    XMLSEC_SAFE_CAST_SIZE_TO_INT(bufferSize, bufferLen, return(-1), NULL);
    doc = xmlRecoverMemory((const char*)buffer, bufferLen);
    if(doc == NULL) {
        xmlSecXmlError("xmlRecoverMemory", xmlSecKeyDataKlassGetName(typeId));
        return(-1);
    }

    cur = xmlDocGetRootElement(doc);
    if(cur == NULL) {
        xmlSecXmlError("xmlDocGetRootElement", xmlSecKeyDataKlassGetName(typeId));
        xmlFreeDoc(doc);
        return(-1);
    }

    nodeName = cur->name;
    nodeNs = xmlSecGetNodeNsHref(cur);

    /* use global list only if we don't have a local one */
    if(xmlSecPtrListGetSize(&(keyInfoCtx->enabledKeyData)) > 0) {
        dataId = xmlSecKeyDataIdListFindByNode(&(keyInfoCtx->enabledKeyData),
                            nodeName, nodeNs, xmlSecKeyDataUsageRetrievalMethodNodeXml);
    } else {
        dataId = xmlSecKeyDataIdListFindByNode(xmlSecKeyDataIdsGet(),
                            nodeName, nodeNs, xmlSecKeyDataUsageRetrievalMethodNodeXml);
    }
    if(dataId == xmlSecKeyDataIdUnknown) {
        xmlFreeDoc(doc);

        /* laxi schema validation but application can disable it */
        if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_KEYVALUE_STOP_ON_UNKNOWN_CHILD) != 0) {
            xmlSecUnexpectedNodeError(cur, xmlSecKeyDataKlassGetName(typeId));
            return(-1);
        }
        return(0);
    } else if((typeId != xmlSecKeyDataIdUnknown) && (typeId != dataId) &&
              ((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_RETRMETHOD_STOP_ON_MISMATCH_HREF) != 0)) {

        xmlSecOtherError2(XMLSEC_ERRORS_R_MAX_RETRIEVAL_TYPE_MISMATCH,
                          xmlSecKeyDataKlassGetName(dataId),
                          "typeId=%s", xmlSecErrorsSafeString(xmlSecKeyDataKlassGetName(typeId)));
        xmlFreeDoc(doc);
        return(-1);
    }

    /* read data node */
    ret = xmlSecKeyDataXmlRead(dataId, key, cur, keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError2("xmlSecKeyDataXmlRead",
                             xmlSecKeyDataKlassGetName(typeId),
                             "node=%s",
                             xmlSecErrorsSafeString(xmlSecNodeGetName(cur)));
        xmlFreeDoc(doc);
        return(-1);
    }

    xmlFreeDoc(doc);
    return(0);
}


/**************************************************************************
 *
 *&lt;dsig11:KeyInfoReference/&gt; element  processing
 *
 *************************************************************************/
static int                      xmlSecKeyDataKeyInfoReferenceXmlRead(xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);
static int                      xmlSecKeyDataKeyInfoReferenceXmlWrite(xmlSecKeyDataId id,
                                                                 xmlSecKeyPtr key,
                                                                 xmlNodePtr node,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);



static xmlSecKeyDataKlass xmlSecKeyDataKeyInfoReferenceKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameKeyInfoReference,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml, /* xmlSecKeyDataUsage usage; */
    NULL,                                       /* const xmlChar* href; */
    xmlSecNodeKeyInfoReference,                 /* const xmlChar* dataNodeName; */
    xmlSecDSig11Ns,                             /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataKeyInfoReferenceXmlRead,        /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataKeyInfoReferenceXmlWrite,       /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

static int                      xmlSecKeyDataKeyInfoReferenceReadXmlResult(xmlSecKeyDataId typeId,
                                                                 xmlSecKeyPtr key,
                                                                 const xmlChar* buffer,
                                                                 xmlSecSize bufferSize,
                                                                 xmlSecKeyInfoCtxPtr keyInfoCtx);

/**
 * xmlSecKeyDataKeyInfoReferenceGetKlass:
 *
 * A KeyInfoReference element within KeyInfo is used to convey a reference to
 * a KeyInfo element at another location in the same or different document.
 *
 * KeyInfoReference uses the same syntax and dereferencing behavior as Reference's URI
 * and the Reference Processing Model except that there are no child elements and
 * the presence of the URI attribute is mandatory.
 *
 * The result of dereferencing a KeyInfoReference MUST be a KeyInfo element, or an XML
 * document with a KeyInfo element as the root.
 *
 *      <!-- targetNamespace="http://www.w3.org/2009/xmldsig11#" -->
 *      <element name="KeyInfoReference" type="dsig11:KeyInfoReferenceType"/>
 *      <complexType name="KeyInfoReferenceType">
 *          <attribute name="URI" type="anyURI" use="required"/>
 *          <attribute name="Id" type="ID" use="optional"/>
 *      </complexType>
 *
 * https://www.w3.org/TR/xmldsig-core1/#sec-KeyInfoReference
 *
 * Returns: the&lt;dsig11:KeyInfoReference/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataKeyInfoReferenceGetKlass(void) {
    return(&xmlSecKeyDataKeyInfoReferenceKlass);
}

static int
xmlSecKeyDataKeyInfoReferenceXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyDataId dataId = xmlSecKeyDataIdUnknown;
    xmlChar *uri = NULL;
    xmlNodePtr cur;
    int res = -1;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataKeyInfoReferenceId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(node->doc != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    /* check retrieval level */
    if(keyInfoCtx->curKeyInfoReferenceLevel >= keyInfoCtx->maxKeyInfoReferenceLevel) {
        xmlSecOtherError3(XMLSEC_ERRORS_R_MAX_KEYINFOREFERENCE_LEVEL, xmlSecKeyDataKlassGetName(id),
            "cur=%d;max=%d",keyInfoCtx->curKeyInfoReferenceLevel, keyInfoCtx->maxKeyInfoReferenceLevel);
        goto done;
    }
    ++keyInfoCtx->curKeyInfoReferenceLevel;

    /* uri attribute is required */
    uri = xmlGetProp(node, xmlSecAttrURI);
    if(uri == NULL) {
        xmlSecInvalidNodeAttributeError(node, xmlSecAttrURI, xmlSecKeyDataKlassGetName(id), "empty");
        goto done;
    }

    /* destroy prev retrieval method context if any and set start URI */
    xmlSecTransformCtxReset(&(keyInfoCtx->keyInfoReferenceCtx));
    ret = xmlSecTransformCtxSetUri(&(keyInfoCtx->keyInfoReferenceCtx), uri, node);
    if(ret < 0) {
        xmlSecInternalError2("xmlSecTransformCtxSetUri", xmlSecKeyDataKlassGetName(id),
            "uri=%s", xmlSecErrorsSafeString(uri));
        goto done;
    }

    /* no children are expected */
    cur = xmlSecGetNextElementNode(node->children);
    if(cur != NULL) {
        xmlSecUnexpectedNodeError(cur, xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    /* get transforms results */
    ret = xmlSecTransformCtxExecute(&(keyInfoCtx->keyInfoReferenceCtx), node->doc);
    if(
        (ret < 0) ||
        (keyInfoCtx->keyInfoReferenceCtx.result == NULL) ||
        (xmlSecBufferGetData(keyInfoCtx->keyInfoReferenceCtx.result) == NULL)
    ) {

        xmlSecInternalError("xmlSecTransformCtxExecute", xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    /* The result of dereferencing a KeyInfoReference MUST be a KeyInfo element,
     * or an XML document with a KeyInfo element as the root */
    ret = xmlSecKeyDataKeyInfoReferenceReadXmlResult(dataId, key,
                    xmlSecBufferGetData(keyInfoCtx->keyInfoReferenceCtx.result),
                    xmlSecBufferGetSize(keyInfoCtx->keyInfoReferenceCtx.result),
                    keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyDataKeyInfoReferenceReadXmlResult", xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    --keyInfoCtx->curKeyInfoReferenceLevel;
    res = 0;

done:
    if(uri != NULL) {
        xmlFree(uri);
    }
    return(res);
}

static int
xmlSecKeyDataKeyInfoReferenceXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecAssert2(id == xmlSecKeyDataKeyInfoReferenceId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    /* just do nothing */
    return(0);
}

static int
xmlSecKeyDataKeyInfoReferenceReadXmlResult(xmlSecKeyDataId typeId, xmlSecKeyPtr key,
                                          const xmlChar* buffer, xmlSecSize bufferSize,
                                          xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlDocPtr doc;
    xmlNodePtr cur;
    int bufferLen;
    int ret;

    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(buffer != NULL, -1);
    xmlSecAssert2(bufferSize > 0, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);


    XMLSEC_SAFE_CAST_SIZE_TO_INT(bufferSize, bufferLen, return(-1), NULL);
    doc = xmlRecoverMemory((const char*)buffer, bufferLen);
    if(doc == NULL) {
        xmlSecXmlError("xmlRecoverMemory", xmlSecKeyDataKlassGetName(typeId));
        return(-1);
    }

    cur = xmlDocGetRootElement(doc);
    if(cur == NULL) {
        xmlSecXmlError("xmlDocGetRootElement", xmlSecKeyDataKlassGetName(typeId));
        xmlFreeDoc(doc);
        return(-1);
    }

    /* The result of dereferencing a KeyInfoReference MUST be a KeyInfo element, or
     * an XML document with a KeyInfo element as the root */
    if(!xmlSecCheckNodeName(cur, xmlSecNodeKeyInfo, xmlSecDSigNs)) {
        xmlSecInvalidNodeError(cur, xmlSecNodeKeyInfo, xmlSecKeyDataKlassGetName(typeId));
        xmlFreeDoc(doc);
        return(-1);
    }

    ret = xmlSecKeyInfoNodeRead(cur, key, keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoNodeRead", xmlSecKeyDataKlassGetName(typeId));
        xmlFreeDoc(doc);
        return(-1);
    }

    /* success */
    xmlFreeDoc(doc);
    return(0);
}


#ifndef XMLSEC_NO_XMLENC

/**************************************************************************
 *
 * &lt;enc:EncryptedKey/&gt; processing
 *
 *************************************************************************/
static int      xmlSecKeyDataEncryptedKeyXmlRead        (xmlSecKeyDataId id,
                                                         xmlSecKeyPtr key,
                                                         xmlNodePtr node,
                                                         xmlSecKeyInfoCtxPtr keyInfoCtx);
static int      xmlSecKeyDataEncryptedKeyXmlWrite       (xmlSecKeyDataId id,
                                                         xmlSecKeyPtr key,
                                                         xmlNodePtr node,
                                                         xmlSecKeyInfoCtxPtr keyInfoCtx);



static xmlSecKeyDataKlass xmlSecKeyDataEncryptedKeyKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameEncryptedKey,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml,
                                                /* xmlSecKeyDataUsage usage; */
    xmlSecHrefEncryptedKey,                     /* const xmlChar* href; */
    xmlSecNodeEncryptedKey,                     /* const xmlChar* dataNodeName; */
    xmlSecEncNs,                                /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataEncryptedKeyXmlRead,           /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataEncryptedKeyXmlWrite,          /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

/**
 * xmlSecKeyDataEncryptedKeyGetKlass:
 *
 * The &lt;enc:EncryptedKey/&gt; element key data klass
 * (http://www.w3.org/TR/xmlenc-core/#sec-EncryptedKey):
 *
 * The EncryptedKey element is used to transport encryption keys from
 * the originator to a known recipient(s). It may be used as a stand-alone
 * XML document, be placed within an application document, or appear inside
 * an EncryptedData element as a child of a ds:KeyInfo element. The key value
 * is always encrypted to the recipient(s). When EncryptedKey is decrypted the
 * resulting octets are made available to the EncryptionMethod algorithm
 * without any additional processing.
 *
 * Returns: the &lt;enc:EncryptedKey/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataEncryptedKeyGetKlass(void) {
    return(&xmlSecKeyDataEncryptedKeyKlass);
}

static int
xmlSecKeyDataEncryptedKeyXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecBufferPtr result;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataEncryptedKeyId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    /* check the enc level */
    if(keyInfoCtx->curEncryptedKeyLevel >= keyInfoCtx->maxEncryptedKeyLevel) {
        xmlSecOtherError3(XMLSEC_ERRORS_R_MAX_ENCKEY_LEVEL, xmlSecKeyDataKlassGetName(id),
            "cur=%d;max=%d", keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
        return(-1);
    }

    /* init Enc context */
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxReset(keyInfoCtx->encCtx);
    } else {
        ret = xmlSecKeyInfoCtxCreateEncCtx(keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyInfoCtxCreateEncCtx", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
    }
    xmlSecAssert2(keyInfoCtx->encCtx != NULL, -1);

    /* copy prefs */
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoReadCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(readCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoWriteCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(writeCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    /* decrypt */
    ++keyInfoCtx->curEncryptedKeyLevel;
    result = xmlSecEncCtxDecryptToBuffer(keyInfoCtx->encCtx, node);
    --keyInfoCtx->curEncryptedKeyLevel;
    if((result == NULL) || (xmlSecBufferGetData(result) == NULL)) {
        /* We might have multiple EncryptedKey elements, encrypted
         * for different recipients but application can enforce
         * correct enc key.
         */
        if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_ENCKEY_DONT_STOP_ON_FAILED_DECRYPTION) != 0) {
            xmlSecInternalError("xmlSecEncCtxDecryptToBuffer", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
        return(0);
    }

    ret = xmlSecKeyDataBinRead(keyInfoCtx->keyReq.keyId, key,
                           xmlSecBufferGetData(result),
                           xmlSecBufferGetSize(result),
                           keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyDataBinRead",
                            xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    return(0);
}

static int
xmlSecKeyDataEncryptedKeyXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyInfoCtx keyInfoCtx2;
    xmlSecByte *keyBuf = NULL;
    xmlSecSize keySize = 0;
    int res = -1;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataEncryptedKeyId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(xmlSecKeyIsValid(key), -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    /* dump key to a binary buffer */
    ret = xmlSecKeyInfoCtxInitialize(&keyInfoCtx2, NULL);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxInitialize",
                            xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    ret = xmlSecKeyInfoCtxCopyUserPref(&keyInfoCtx2, keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref", xmlSecKeyDataKlassGetName(id));
        xmlSecKeyInfoCtxFinalize(&keyInfoCtx2);
        goto done;
    }

    keyInfoCtx2.keyReq.keyType = xmlSecKeyDataTypeAny;
    ret = xmlSecKeyDataBinWrite(key->value->id, key, &keyBuf, &keySize, &keyInfoCtx2);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyDataBinWrite", xmlSecKeyDataKlassGetName(id));
        xmlSecKeyInfoCtxFinalize(&keyInfoCtx2);
        goto done;
    }
    xmlSecKeyInfoCtxFinalize(&keyInfoCtx2);

    /* init Enc context */
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxReset(keyInfoCtx->encCtx);
    } else {
        ret = xmlSecKeyInfoCtxCreateEncCtx(keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyInfoCtxCreateEncCtx", xmlSecKeyDataKlassGetName(id));
            goto done;
        }
    }
    xmlSecAssert2(keyInfoCtx->encCtx != NULL, -1);

    /* copy prefs */
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoReadCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(readCtx)", xmlSecKeyDataKlassGetName(id));
        goto done;
    }
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoWriteCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(writeCtx)", xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    /* encrypt */
    ret = xmlSecEncCtxBinaryEncrypt(keyInfoCtx->encCtx, node, keyBuf, keySize);
    if(ret < 0) {
        xmlSecInternalError("xmlSecEncCtxBinaryEncrypt", xmlSecKeyDataKlassGetName(id));
        goto done;
    }

    res = 0;
done:
    if(keyBuf != NULL) {
        memset(keyBuf, 0, keySize);
        xmlFree(keyBuf); keyBuf = NULL;
    }
    return(res);
}


/**************************************************************************
 *
 *&lt;enc11:DerivedKey/&gt; element processing
 *
 *************************************************************************/
static int      xmlSecKeyDataDerivedKeyXmlRead          (xmlSecKeyDataId id,
                                                         xmlSecKeyPtr key,
                                                         xmlNodePtr node,
                                                         xmlSecKeyInfoCtxPtr keyInfoCtx);
static int      xmlSecKeyDataDerivedKeyXmlWrite         (xmlSecKeyDataId id,
                                                         xmlSecKeyPtr key,
                                                         xmlNodePtr node,
                                                         xmlSecKeyInfoCtxPtr keyInfoCtx);



static xmlSecKeyDataKlass xmlSecKeyDataDerivedKeyKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameDerivedKey,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml,
                                                /* xmlSecKeyDataUsage usage; */
    xmlSecHrefDerivedKey,                       /* const xmlChar* href; */
    xmlSecNodeDerivedKey,                       /* const xmlChar* dataNodeName; */
    xmlSecEnc11Ns,                              /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataDerivedKeyXmlRead,             /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataDerivedKeyXmlWrite,            /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

/**
 * xmlSecKeyDataDerivedKeyGetKlass:
 *
 * The&lt;enc11:DerivedKey/&gt; element key data klass
 * (https://www.w3.org/TR/xmlenc-core1/#sec-DerivedKey)
 *
 * The DerivedKey element is used to transport information about
 * a derived key from the originator to recipient(s). It may be
 * used as a stand-alone XML document, be placed within an application
 * document, or appear inside an EncryptedData or Signature element as
 * a child of a ds:KeyInfo element. The key value itself is never sent
 * by the originator. Rather, the originator provides information to
 * the recipient(s) by which the recipient(s) can derive the same key value.
 * When the key has been derived the resulting octets are made available
 * to the EncryptionMethod or SignatureMethod algorithm without
 * any additional processing.
 *
 * Returns: the&lt;enc11:DerivedKey/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataDerivedKeyGetKlass(void) {
    return(&xmlSecKeyDataDerivedKeyKlass);
}

static int
xmlSecKeyDataDerivedKeyXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyPtr generatedKey;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataDerivedKeyId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    /* check the enc level */
    if(keyInfoCtx->curEncryptedKeyLevel >= keyInfoCtx->maxEncryptedKeyLevel) {
        xmlSecOtherError3(XMLSEC_ERRORS_R_MAX_ENCKEY_LEVEL, xmlSecKeyDataKlassGetName(id),
            "cur=%d;max=%d", keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
        return(-1);
    }

    /* init enc context */
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxReset(keyInfoCtx->encCtx);
    } else {
        ret = xmlSecKeyInfoCtxCreateEncCtx(keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyInfoCtxCreateEncCtx", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
    }
    xmlSecAssert2(keyInfoCtx->encCtx != NULL, -1);

    /* copy prefs */
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoReadCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(readCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoWriteCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(writeCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    ++keyInfoCtx->curEncryptedKeyLevel;
    generatedKey = xmlSecEncCtxDerivedKeyGenerate(keyInfoCtx->encCtx, keyInfoCtx->keyReq.keyId, node, keyInfoCtx);
    --keyInfoCtx->curEncryptedKeyLevel;

    if(generatedKey == NULL) {
        /* We might have multiple DerivedKey elements, encrypted
         * for different recipients but application can enforce
         * correct enc key.
         */
        if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_ENCKEY_DONT_STOP_ON_FAILED_DECRYPTION) != 0) {
            xmlSecInternalError("xmlSecEncCtxDerivedKeyGenerate", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
        return(0);
    }

    /* TODO: store derived keys in keyInfoCtx so one can reference
     * the key by name from ds:KeyName
     * BTW this should happen even if the key doesn't match the current request
     * (https://github.com/lsh123/xmlsec/issues/515)
     */
    if(xmlSecKeyReqMatchKey(&(keyInfoCtx->keyReq), generatedKey) != 1) {
        /* we are not allowed to use  this key, ignore and continue */
        xmlSecKeyDestroy(generatedKey);
        return(0);
    }

    ret = xmlSecKeyCopy(key, generatedKey);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyCopy", xmlSecKeyDataKlassGetName(id));
        xmlSecKeyDestroy(generatedKey);
        return(-1);
    }

    /* success */
    xmlSecKeyDestroy(generatedKey);
    return(0);
}

static int
xmlSecKeyDataDerivedKeyXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecAssert2(id == xmlSecKeyDataDerivedKeyId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(xmlSecKeyIsValid(key), -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    /* do nothing, the template should already have all the necesary data to generate the key correctly */
    return(0);
}


/**************************************************************************
 *
 * &lt;enc:AgreementMethod/&gt; processing
 *
 *************************************************************************/
static int      xmlSecKeyDataAgreementMethodXmlRead     (xmlSecKeyDataId id,
                                                         xmlSecKeyPtr key,
                                                         xmlNodePtr node,
                                                         xmlSecKeyInfoCtxPtr keyInfoCtx);
static int      xmlSecKeyDataAgreementMethodXmlWrite    (xmlSecKeyDataId id,
                                                         xmlSecKeyPtr key,
                                                         xmlNodePtr node,
                                                         xmlSecKeyInfoCtxPtr keyInfoCtx);



static xmlSecKeyDataKlass xmlSecKeyDataAgreementMethodKlass = {
    sizeof(xmlSecKeyDataKlass),
    sizeof(xmlSecKeyData),

    /* data */
    xmlSecNameAgreementMethod,
    xmlSecKeyDataUsageKeyInfoNode | xmlSecKeyDataUsageRetrievalMethodNodeXml,
                                                /* xmlSecKeyDataUsage usage; */
    xmlSecHrefAgreementMethod,                  /* const xmlChar* href; */
    xmlSecNodeAgreementMethod,                  /* const xmlChar* dataNodeName; */
    xmlSecEncNs,                                /* const xmlChar* dataNodeNs; */

    /* constructors/destructor */
    NULL,                                       /* xmlSecKeyDataInitializeMethod initialize; */
    NULL,                                       /* xmlSecKeyDataDuplicateMethod duplicate; */
    NULL,                                       /* xmlSecKeyDataFinalizeMethod finalize; */
    NULL,                                       /* xmlSecKeyDataGenerateMethod generate; */

    /* get info */
    NULL,                                       /* xmlSecKeyDataGetTypeMethod getType; */
    NULL,                                       /* xmlSecKeyDataGetSizeMethod getSize; */
    NULL,                                       /* xmlSecKeyDataGetIdentifier getIdentifier; */

    /* read/write */
    xmlSecKeyDataAgreementMethodXmlRead,        /* xmlSecKeyDataXmlReadMethod xmlRead; */
    xmlSecKeyDataAgreementMethodXmlWrite,       /* xmlSecKeyDataXmlWriteMethod xmlWrite; */
    NULL,                                       /* xmlSecKeyDataBinReadMethod binRead; */
    NULL,                                       /* xmlSecKeyDataBinWriteMethod binWrite; */

    /* debug */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugDump; */
    NULL,                                       /* xmlSecKeyDataDebugDumpMethod debugXmlDump; */

    /* reserved for the future */
    NULL,                                       /* void* reserved0; */
    NULL,                                       /* void* reserved1; */
};

/**
 * xmlSecKeyDataAgreementMethodGetKlass:
 *
 * The &lt;enc:AgreementMethod/&gt; element key data klass
 * (hhttps://www.w3.org/TR/xmlenc-core1/#sec-Alg-KeyAgreement)
 *
 * A Key Agreement algorithm provides for the derivation of a shared secret key based on
 * a shared secret computed from certain types of compatible public keys from both the sender
 * and the recipient. Information from the originator to determine the secret is indicated by
 * an optional OriginatorKeyInfo parameter child of an AgreementMethod element while that associated
 * with the recipient is indicated by an optional RecipientKeyInfo. A shared key is derived from
 * this shared secret by a method determined by the Key Agreement algorithm.
 *
 * Returns: the &lt;enc:AgreementMethod/&gt; element processing key data klass.
 */
xmlSecKeyDataId
xmlSecKeyDataAgreementMethodGetKlass(void) {
    return(&xmlSecKeyDataAgreementMethodKlass);
}

static int
xmlSecKeyDataAgreementMethodXmlRead(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyPtr generatedKey;
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataAgreementMethodId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeRead, -1);

    /* check the enc level */
    if(keyInfoCtx->curEncryptedKeyLevel >= keyInfoCtx->maxEncryptedKeyLevel) {
        xmlSecOtherError3(XMLSEC_ERRORS_R_MAX_ENCKEY_LEVEL, xmlSecKeyDataKlassGetName(id),
            "cur=%d;max=%d", keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
        return(-1);
    }

    /* init enc context */
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxReset(keyInfoCtx->encCtx);
    } else {
        ret = xmlSecKeyInfoCtxCreateEncCtx(keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyInfoCtxCreateEncCtx", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
    }
    xmlSecAssert2(keyInfoCtx->encCtx != NULL, -1);

    /* copy prefs */
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoReadCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(readCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoWriteCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(writeCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    ++keyInfoCtx->curEncryptedKeyLevel;
    generatedKey = xmlSecEncCtxAgreementMethodGenerate(keyInfoCtx->encCtx, keyInfoCtx->keyReq.keyId, node, keyInfoCtx);
    --keyInfoCtx->curEncryptedKeyLevel;

    if(generatedKey == NULL) {
        /* We might have multiple AgreementMethod elements, encrypted
         * for different recipients but application can enforce
         * correct enc key.
         */
        if((keyInfoCtx->flags & XMLSEC_KEYINFO_FLAGS_ENCKEY_DONT_STOP_ON_FAILED_DECRYPTION) != 0) {
            xmlSecInternalError("xmlSecEncCtxAgreementMethodGenerate", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
        return(0);
    }

    if(xmlSecKeyReqMatchKey(&(keyInfoCtx->keyReq), generatedKey) != 1) {
        /* we are not allowed to use  this key, ignore and continue */
        xmlSecKeyDestroy(generatedKey);
        return(0);
    }

    ret = xmlSecKeyCopy(key, generatedKey);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyCopy", xmlSecKeyDataKlassGetName(id));
        xmlSecKeyDestroy(generatedKey);
        return(-1);
    }

    /* success */
    xmlSecKeyDestroy(generatedKey);
    return(0);
}

static int
xmlSecKeyDataAgreementMethodXmlWrite(xmlSecKeyDataId id, xmlSecKeyPtr key, xmlNodePtr node, xmlSecKeyInfoCtxPtr keyInfoCtx) {
    int ret;

    xmlSecAssert2(id == xmlSecKeyDataAgreementMethodId, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(xmlSecKeyIsValid(key), -1);
    xmlSecAssert2(node != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2(keyInfoCtx->mode == xmlSecKeyInfoModeWrite, -1);

    /* there might be several nodes that can re-use encCtx, we need to re-read the node before writing it  */

    /* check the enc level */
    if(keyInfoCtx->curEncryptedKeyLevel >= keyInfoCtx->maxEncryptedKeyLevel) {
        xmlSecOtherError3(XMLSEC_ERRORS_R_MAX_ENCKEY_LEVEL, xmlSecKeyDataKlassGetName(id),
            "cur=%d;max=%d", keyInfoCtx->curEncryptedKeyLevel, keyInfoCtx->maxEncryptedKeyLevel);
        return(-1);
    }

    /* init enc context */
    if(keyInfoCtx->encCtx != NULL) {
        xmlSecEncCtxReset(keyInfoCtx->encCtx);
    } else {
        ret = xmlSecKeyInfoCtxCreateEncCtx(keyInfoCtx);
        if(ret < 0) {
            xmlSecInternalError("xmlSecKeyInfoCtxCreateEncCtx", xmlSecKeyDataKlassGetName(id));
            return(-1);
        }
    }
    xmlSecAssert2(keyInfoCtx->encCtx != NULL, -1);

    /* copy prefs */
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoReadCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(readCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }
    ret = xmlSecKeyInfoCtxCopyUserPref(&(keyInfoCtx->encCtx->keyInfoWriteCtx), keyInfoCtx);
    if(ret < 0) {
        xmlSecInternalError("xmlSecKeyInfoCtxCopyUserPref(writeCtx)", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    ++keyInfoCtx->curEncryptedKeyLevel;
    ret = xmlSecEncCtxAgreementMethodXmlWrite(keyInfoCtx->encCtx, node, keyInfoCtx);
    --keyInfoCtx->curEncryptedKeyLevel;

    if(ret < 0) {
        xmlSecInternalError("xmlSecEncCtxAgreementMethodXmlWrite", xmlSecKeyDataKlassGetName(id));
        return(-1);
    }

    return(0);
}


#endif /* XMLSEC_NO_XMLENC */
