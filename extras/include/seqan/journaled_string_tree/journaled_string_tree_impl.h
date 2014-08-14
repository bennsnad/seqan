// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2014, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Rene Rahn <rene.rahn@fu-berlin.de>
// ==========================================================================
// Implements the default virtual string tree to traverse multiple sequences
// in parallel. This is a facade combining the variant store with the
// journal set.
// ==========================================================================

#ifndef EXTRAS_INCLUDE_SEQAN_JOURNALED_STRING_TREE_JOURNALED_STRING_TREE_IMPL_H_
#define EXTRAS_INCLUDE_SEQAN_JOURNALED_STRING_TREE_JOURNALED_STRING_TREE_IMPL_H_

namespace seqan
{

// ============================================================================
// Forwards
// ============================================================================

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

template <typename T>
struct GetStringSet{};

// ----------------------------------------------------------------------------
// Class JournaledStringTree                                [StringTreeDefault]
// ----------------------------------------------------------------------------

/*!
 * @class JournaledStringTree Journaled String Tree
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief A dynamic data structure representing a set of similar sequences in succinct form.
 *
 * @signature template <typename TDelta[, typename TSpec]>
 *            class JournaledStringTree<TDeltaStore, TSpec>;
 *
 * @tparam TDelta Type of the object storing delta information for a set of sequences. Of type @link DeltaMap @endlink.
 * @tparam TSpec The journaled string tree type. Defaults to @link Default @endlink.
 *
 * This data structure stores delta information between a set of sequences and a common reference sequence. The
 * @link DeltaMap @endlink is used to store these information efficiently.
 *
 * In addition, the journaled string tree manages a set of referentially compressed strings @link JournaledSet @endlink
 * to represent the sequences encoded by the the delta map. The sequence information is built on-demand and can
 * be constructed dynamically in blocks.
 *
 * Use the function @link JournaledStringTree#create @endlink and @link JournaledStringTree#createNext @endlink to force
 * the creation of the sequence information for the next block.
 * Per default the entire sequence set is build. To build the sequences in blocks
 * over the mapped variants use the function @link JournaledStringTree#setBlockSize @endlink. The block size
 * is the number of variants that should be integrated in the current sequence content.
 * Note that when generating the sequences block-wise, the actual position of each sequence depends only on the
 * currently integrated variants. Use the function @link JournaledStringTree#virtualBlockOffset @endlink to get
 * the offset for the previously built blocks. Adding this offset to the current sequence position gives the
 * global sequence position.
 *
 * The sequences represented by the delta map and the journaled strings can be traversed in forward direction.
 * To do so one need to instantiate a @link JstTraverser @endlink object, which manages the traversal over the
 * Journaled String Tree. To communicate with the Journaled String Tree during the traversal the caller of the
 * @link JstTraverser#traverse @endlink has to implement the @link JstTraversalConcept @endlink.
 */

template <typename TDeltaMap>
class JournaledStringTree<TDeltaMap, StringTreeDefault>
{
public:
    typedef typename GetStringSet<JournaledStringTree>::Type    TJournalData;
    typedef typename Value<TJournalData>::Type                  TJournaledString;
    typedef typename Size<TJournalData>::Type                   TSize;
    typedef typename MakeSigned<TSize>::Type                    TSignedSize;
    typedef typename Container<JournaledStringTree>::Type       TContainer;
    typedef typename Iterator<TContainer, Standard>::Type       TMapIterator;

    // TODO(rmaerker): Maybe no holder.
    Holder<TDeltaMap>               _container;

    mutable TJournalData            _journalSet;
    mutable String<TSignedSize>     _blockVPOffset;  // Virtual position offset for each sequence visited so far.
    mutable String<TSignedSize>     _activeBlockVPOffset;  // Virtual position offset for each sequence visited so far.
    mutable TSize                   _activeBlock;
    mutable TMapIterator            _mapBlockBegin;
    mutable TMapIterator            _mapBlockEnd;
    mutable bool                    _emptyJournal;

    TSize                           _blockSize;
    TSize                           _numBlocks;
    static const TSize REQUIRE_FULL_JOURNAL = MaxValue<TSize>::VALUE;

    JournaledStringTree() : _container(),
                            _journalSet(),
                            _activeBlock(0),
                            _mapBlockBegin(),
                            _mapBlockEnd(),
                            _emptyJournal(true),
                            _blockSize(REQUIRE_FULL_JOURNAL),
                            _numBlocks(1)
    {
        create(_container);
    }

    template <typename THost>
    JournaledStringTree(THost & reference, TDeltaMap & varData) : _activeBlock(0),
                                                                  _mapBlockBegin(),
                                                                  _mapBlockEnd(),
                                                                  _emptyJournal(true),
                                                                  _blockSize(REQUIRE_FULL_JOURNAL),
                                                                  _numBlocks(1)
    {
        init(*this, reference, varData);
    }
};

// ============================================================================
// Metafunctions
// ============================================================================

// ----------------------------------------------------------------------------
// Metafunction Spec
// ----------------------------------------------------------------------------

/*!
 * @mfn JournaledStringTree#Spec
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns the specialization type.
 *
 * @signature Spec<TJst>::Type;
 *
 * @tparam TJst The type of the journaled string tree to get the specialization type for.
 *
 * @return TSpec The specialization type.
 */

template <typename TDeltaMap, typename TSpec>
struct Spec<JournaledStringTree<TDeltaMap, TSpec> >
{
    typedef TSpec Type;
};

template <typename TDeltaMap, typename TSpec>
struct Spec<JournaledStringTree<TDeltaMap, TSpec> const> :
    Spec<JournaledStringTree<TDeltaMap, TSpec> >{};

// ----------------------------------------------------------------------------
// Metafunction Position
// ----------------------------------------------------------------------------

/*!
 * @mfn JournaledStringTree#Position
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief The position type.
 *
 * @signature Position<TJst>::Type;
 *
 * @tparam TJst The type of the journal string tree to get the position type for.
 *
 * @return TPosition The position type.
 */

template <typename TDeltaMap, typename TSpec>
struct Position<JournaledStringTree<TDeltaMap, TSpec> > :
    Position<TDeltaMap>{};

template <typename TDeltaMap, typename TSpec>
struct Position<JournaledStringTree<TDeltaMap, TSpec> const> :
    Position<TDeltaMap const>{};

// ----------------------------------------------------------------------------
// Metafunction Size
// ----------------------------------------------------------------------------

/*!
 * @mfn JournaledStringTree#Size
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief The size type.
 *
 * @signature Size<TJst>::Type;
 *
 * @tparam TJst The type of the journal string tree to get the size type for.
 *
 * @return TSize The size type.
 */

template <typename TDeltaMap, typename TSpec>
struct Size<JournaledStringTree<TDeltaMap, TSpec> > :
    Size<TDeltaMap>{};

template <typename TDeltaMap, typename TSpec>
struct Size<JournaledStringTree<TDeltaMap, TSpec> const> :
    Size<TDeltaMap const>{};

// ----------------------------------------------------------------------------
// Metafunction Host
// ----------------------------------------------------------------------------

/*!
 * @mfn JournaledStringTree#Host
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns the type of the global reference.
 *
 * @signature Host<TJst>::Type;
 *
 * @tparam TJst The type of the journal string tree to get the host type for.
 *
 * @return THost The type of the global reference sequence.
 */

template <typename TDeltaMap, typename TSpec>
struct Host<JournaledStringTree<TDeltaMap, TSpec> >
{
    typedef JournaledStringTree<TDeltaMap, TSpec> TVStringTree_;
    typedef typename GetStringSet<TVStringTree_>::Type TJournalData_;
    typedef typename Host<TJournalData_>::Type Type;
};

template <typename TDeltaMap, typename TSpec>
struct Host<JournaledStringTree<TDeltaMap, TSpec> const>
{
    typedef JournaledStringTree<TDeltaMap, TSpec> TVStringTree_;
    typedef typename Host<TVStringTree_>::Type const Type;
};

// ----------------------------------------------------------------------------
// Metafunction Container
// ----------------------------------------------------------------------------

/*!
 * @mfn JournaledStringTree#Container
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns the type of the branch node map holding the delta information.
 *
 * @signature Container<TJst>::Type;
 *
 * @tparam TJst The type of the journal string tree to get the branch node map type for.
 *
 * @return TContainer The branch node map containing the delta information.
 */

template <typename TDeltaMap, typename TSpec>
struct Container<JournaledStringTree<TDeltaMap, TSpec> >
{
    typedef TDeltaMap Type;
};

template <typename TDeltaMap, typename TSpec>
struct Container<JournaledStringTree<TDeltaMap, TSpec> const>
{
    typedef TDeltaMap const Type;
};

// ----------------------------------------------------------------------------
// Metafunction GetStringSet
// ----------------------------------------------------------------------------

/*!
 * @mfn JournaledStringTree#GetStringSet
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns the type of the string set holding the compressed sequences.
 *
 * @signature GetStringSet<TJst>::Type;
 *
 * @tparam TJst The type of the journaled string tree to get the string set type for.
 *
 * @return TStringSet The string set containing the constructed sequences.
 */

template <typename TDeltaMap, typename TSpec>
struct GetStringSet<JournaledStringTree<TDeltaMap, TSpec> >
{
    typedef typename DeltaValue<TDeltaMap, DeltaType::DELTA_TYPE_SNP>::Type TAlphabet_;

    typedef String<TAlphabet_, Journaled<Alloc<>, SortedArray> > TString_;
    typedef StringSet<TString_, Owner<JournaledSet> > Type;
};

template <typename TDeltaMap, typename TSpec>
struct GetStringSet<JournaledStringTree<TDeltaMap, TSpec> const>
{
    typedef typename DeltaValue<TDeltaMap, DeltaType::DELTA_TYPE_SNP>::Type TAlphabet_;

    typedef String<TAlphabet_, Journaled<Alloc<>, SortedArray> > TString_;
    typedef StringSet<TString_, Owner<JournaledSet> > const Type;
};


// ============================================================================
// Functions
// ============================================================================

// ----------------------------------------------------------------------------
// Function _journalNextVariant()
// ----------------------------------------------------------------------------

template <typename TJournalString, typename TMapIter>
void _journalNextVariant(TJournalString & jString, TMapIter const & it)
{
    switch(deltaType(it))
    {
        case DeltaType::DELTA_TYPE_SNP:
            _journalSnp(jString, *it, deltaSnp(it));
            break;
        case DeltaType::DELTA_TYPE_DEL:
            _journalDel(jString, *it, deltaDel(it));
            break;
        case DeltaType::DELTA_TYPE_INS:
            _journalIns(jString, *it, deltaIns(it));
            break;
        case DeltaType::DELTA_TYPE_INDEL:
            _journalIndel(jString, *it, deltaIndel(it));
            break;
    }
}

// ----------------------------------------------------------------------------
// Function _doJournalBlock()
// ----------------------------------------------------------------------------

template <typename TDeltaMap, typename TSpec, typename TContextSize, typename TParallelTag>
inline bool
_doJournalBlock(JournaledStringTree<TDeltaMap, TSpec> const & jst,
                TContextSize contextSize,
                Tag<TParallelTag> parallelTag = Serial())
{
    typedef JournaledStringTree<TDeltaMap, TSpec > TJst;
    typedef typename Iterator<TDeltaMap const, Standard>::Type TConstMapIterator;

    typedef typename DeltaCoverage<TDeltaMap>::Type TBitVec;
    typedef typename Iterator<TBitVec const, Standard>::Type TBitVecIter;

    typedef typename GetStringSet<TJst>::Type TJournalSet;
    typedef typename Iterator<TJournalSet, Standard>::Type TJournalSetIter;
    typedef typename Size<TJournalSet>::Type TSize;
    typedef typename MakeSigned<TSize>::Type TSignedSize;

    // Define the block limits.
    if ((jst._activeBlock * jst._blockSize) >= length(container(jst)))
        return false;

    unsigned lastBlock = jst._activeBlock * jst._blockSize;
    TSize blockJump = _min(length(container(jst)), lastBlock + jst._blockSize) - lastBlock;

    // Auxiliary variables.
    TDeltaMap const & variantMap = container(jst);

    String<int> _lastVisitedNodes;
    if (!fullJournalRequired(jst))
    {
        resize(_lastVisitedNodes, length(jst._journalSet), -1, Exact());
        blockJump -= _max(0, (static_cast<TSignedSize>(jst._mapBlockEnd - jst._mapBlockBegin) - static_cast<TSignedSize>(jst._blockSize)));
    }

    TConstMapIterator itMapBegin = begin(variantMap, Standard());
    jst._mapBlockBegin = jst._mapBlockEnd;
    jst._mapBlockEnd += blockJump;
    for (; jst._mapBlockEnd != end(variantMap, Standard()) && *jst._mapBlockEnd == *(jst._mapBlockEnd - 1); ++jst._mapBlockEnd);
    if (jst._mapBlockBegin == jst._mapBlockEnd)
        return false;

    // Use parallel processing.
    // TODO(rmaerker): Consider more general Master-Worker design for parallelization?
    Splitter<TJournalSetIter> jSetSplitter(begin(jst._journalSet, Standard()), end(jst._journalSet, Standard()), parallelTag);

    SEQAN_OMP_PRAGMA(parallel for)
    for (unsigned jobId = 0; jobId < length(jSetSplitter); ++jobId)
    {
//        printf("Thread: %i of %i\n", jobId, omp_get_num_threads());
        unsigned jobBegin = jSetSplitter[jobId] - begin(jst._journalSet, Standard());
        unsigned jobEnd = jSetSplitter[jobId + 1] - begin(jst._journalSet, Standard());

        // Pre-processing: Update VPs for last block.
        if (!fullJournalRequired(jst))
            for (unsigned i = jobBegin; i < jobEnd; ++i)
            {
                clear(jst._journalSet[i]);  // Reinitialize the journal strings.
                jst._blockVPOffset[i] += jst._activeBlockVPOffset[i];
            }

//        printf("Thread %i: jobBegin %i - jobEnd %i\n", jobId, jobBegin, jobEnd);
        for (TConstMapIterator itMap = jst._mapBlockBegin; itMap != jst._mapBlockEnd; ++itMap)
        {
            TBitVecIter itVecBegin = begin(deltaCoverage(itMap), Standard());
            TBitVecIter itVec = itVecBegin + jobBegin;
            TBitVecIter itVecEnd = begin(deltaCoverage(itMap), Standard()) + jobEnd;
            for (;itVec != itVecEnd; ++itVec)
            {
                SEQAN_ASSERT_NOT(empty(host(jst._journalSet[itVec - itVecBegin])));

                if (!(*itVec))
                    continue;

                // Store last visited node for current journaled string.
                if (!fullJournalRequired(jst))
                    _lastVisitedNodes[itVec - itVecBegin] = itMap - itMapBegin;
                _journalNextVariant(jst._journalSet[itVec - itVecBegin], itMap);
            }
        }

        // Post-processing: Store VPs for current block.
        if (!fullJournalRequired(jst))
        {
            // Buffer the next branch nodes depending on the context size.
            for (unsigned i = jobBegin; i < jobEnd; ++i)
            {
                jst._activeBlockVPOffset[i] = length(jst._journalSet[i]) - length(host(jst._journalSet));
                if (_lastVisitedNodes[i] == -1)  // skip empty journal strings.
                    continue;

                TConstMapIterator tmpMapIt = begin(variantMap, Standard()) + _lastVisitedNodes[i];
                unsigned offset = *tmpMapIt + contextSize;
                if (deltaType(tmpMapIt) == DeltaType::DELTA_TYPE_DEL)
                    offset += deltaDel(tmpMapIt);  // Adds the size of the deletion.
                if (deltaType(tmpMapIt) == DeltaType::DELTA_TYPE_INDEL)
                    offset += deltaIndel(tmpMapIt).i1;

                if (jst._mapBlockEnd != end(variantMap, Standard()) && offset > *jst._mapBlockEnd)
                {
                    tmpMapIt = jst._mapBlockEnd;
                    int localDiff = 0;
                    while (tmpMapIt != end(variantMap, Standard()) && *tmpMapIt < offset + localDiff)
                    {
                        if (deltaCoverage(tmpMapIt)[i])
                        {
                            _journalNextVariant(jst._journalSet[i], tmpMapIt);

                            if (deltaType(tmpMapIt) == DeltaType::DELTA_TYPE_DEL)
                                localDiff += deltaDel(tmpMapIt);
                            else if (deltaType(tmpMapIt) == DeltaType::DELTA_TYPE_INS)
                                localDiff = _max(0, localDiff - static_cast<int>(length(deltaIns(tmpMapIt))));
                            else if (deltaType(tmpMapIt) == DeltaType::DELTA_TYPE_INDEL)
                            {
                                localDiff += deltaIndel(tmpMapIt).i1;
                                localDiff = _max(0, localDiff - static_cast<int>(length(deltaIndel(tmpMapIt).i2)));
                            }
                        }
                        ++tmpMapIt;
                    }
                }
            }
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// Function host()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#host
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns a reference to the global reference sequence.
 *
 * @signature THost host(jst);
 *
 * @param[in] jst    The Journal String Tree.
 *
 * @return THost A reference to the @link JournaledStringTree#Host @endlink object.
 */

template <typename TDeltaMap, typename TSpec>
inline typename Host<JournaledStringTree<TDeltaMap, TSpec> >::Type &
host(JournaledStringTree<TDeltaMap, TSpec> & stringTree)
{
    return host(stringSet(stringTree));
}

template <typename TDeltaMap, typename TSpec>
inline typename Host<JournaledStringTree<TDeltaMap, TSpec> const>::Type &
host(JournaledStringTree<TDeltaMap, TSpec> const & stringTree)
{
    return host(stringSet(stringTree));
}

// ----------------------------------------------------------------------------
// Function virtualBlockOffset()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#virtualBlockOffset
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns the virtual offset for the current block for the given sequence.
 *
 * @signature TSize virtualBlockOffset(jst, id);
 *
 * @param[in] jst    The Journal String Tree.
 * @param[in] id     The id of the sequence to get the offset for.
 *
 * When constructing the sequence information block-wise, an additional offset is used to determine
 * the correct virtual position for the current block for each sequence.
 *
 * @return TSize The Current block offset for the given sequence. The type of the returned value is of type
 * @link MakeSigned @endlink of the @link JournaledStringTree#Size @endlink type.
 */

template <typename TDeltaMap, typename TSpec, typename TPosition>
inline typename MakeSigned<typename Size<JournaledStringTree<TDeltaMap, TSpec> const>::Type>::Type
virtualBlockOffset(JournaledStringTree<TDeltaMap, TSpec> const & stringTree,
                   TPosition const & pos)
{
    return stringTree._blockVPOffset[pos];
}

// ----------------------------------------------------------------------------
// Function fullJournalRequired()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#fullJournalRequired
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Checks whether the sequences are constructed block-wise or not.
 *
 * @signature bool fullJournalRequired(jst);
 *
 * @param[in] jst    The Journal String Tree.
 *
 * @return bool <tt>false</tt> if constructed block-wise, <tt>true</tt> otherwise.
 */

template <typename TDeltaMap, typename TSpec>
inline bool
fullJournalRequired(JournaledStringTree<TDeltaMap, TSpec> const & stringTree)
{
    typedef JournaledStringTree<TDeltaMap, TSpec> TJst;

    return stringTree._blockSize == TJst::REQUIRE_FULL_JOURNAL;
}

// ----------------------------------------------------------------------------
// Function create()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#create
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Creates the journaled strings or the first block if block-wise construction is enabled.
 *
 * @signature bool create(jst, cs[, parallel);
 *
 * @param[in,out] jst       The Journal String Tree to create the journaled strings for.
 * @param[in] cs            The context size.
 * @param[in] parallel      Tag to specialize parallel construction. One of @link ParallelismTags @endlink.
 *                          Defaults to @link ParallelismTags#Serial @endlink.
 *
 * This function creates either the complete journaled strings if @link JournaledStringTree#fullJournalRequired @endlink
 * returns <tt>true<\tt> or the first block if block-wise construction is enabled. To generate the following blocks
 * use the function @link JournaledStringTree#createNext @endlink.
 *
 * @see JournaledStringTree#createNext
 * @see JournaledStringTree#fullJournalRequired
 * @see JournaledStringTree#setBlockSize
 */

template <typename TDeltaMap, typename TSize, typename TParallelTag>
void create(JournaledStringTree<TDeltaMap, StringTreeDefault> & stringTree,
            TSize contextSize,
            Tag<TParallelTag> parallelTag)
{
    typedef JournaledStringTree<TDeltaMap, StringTreeDefault> TJst;

    if (stringTree._blockSize != TJst::REQUIRE_FULL_JOURNAL)  // Require full creation.
    {
        createNext(stringTree, contextSize, parallelTag);
        return;
    }

    if (!stringTree._emptyJournal)  // String tree already created.
        return;
    reinit(stringTree);
    _doJournalBlock(stringTree, contextSize, parallelTag);
    ++stringTree._activeBlock;
    stringTree._emptyJournal = false;
}

template <typename TDeltaMap, typename TSize>
void create(JournaledStringTree<TDeltaMap, StringTreeDefault> const & stringTree,
            TSize contextSize)
{
    create(stringTree, contextSize, Serial());
}

// ----------------------------------------------------------------------------
// Function createNext()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#createNext
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Constructs the sequence content for the next block if available.
 *
 * @signature bool createNext(jst, cs[, tag]);
 *
 * @param[in,out] jst   The Journal String Tree to create the journaled strings for.
 * @param[in] cs        The context size.
 * @param[in] tag       Tag to enable parallel processing. One of @link ParallelismTags @endlink.
 *                      Defaults to @link ParallelismTags#Serial @endlink.
 *
 *
 * @return bool <tt>true</tt> if a new block was generated, <tt>false</tt> otherwise.
 *
 * Before calling this function the function @link JournaledStringTree#create @endlink must be called.
 *
 * @see JournaledStringTree#create
 */

template <typename TDeltaMap, typename TSize, typename TParallelTag>
bool createNext(JournaledStringTree<TDeltaMap, StringTreeDefault> const & stringTree,
                TSize contextSize,
                Tag<TParallelTag> tag)
{
    typedef JournaledStringTree<TDeltaMap, StringTreeDefault> TJst;

    if (stringTree._blockSize == TJst::REQUIRE_FULL_JOURNAL)
        return false;

    bool res = _doJournalBlock(stringTree, contextSize, tag);
    stringTree._emptyJournal = false;
    ++stringTree._activeBlock;
    return res;
}

template <typename TDeltaMap, typename TSpec, typename TSize>
bool createNext(JournaledStringTree<TDeltaMap, TSpec> & stringTree,
                TSize contextSize)
{
    return createNext(stringTree, contextSize, Serial());
}

// ----------------------------------------------------------------------------
// Function reinit()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#reinit
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Reinitializes the journaled string tree.
 *
 * @signature void reinit(jst);
 *
 * @param[in,out] jst    The Journal String Tree.
 *
 * This function resets the tree to the first block. Note, that the journaled strings are not cleard such that in case
 * of no block-wise generation the journaled strings are not reconstructed.
 */

// Keeps the journal strings alive, if the full block has loaded.
template <typename TDeltaMap, typename TSpec>
inline void
reinit(JournaledStringTree<TDeltaMap, TSpec> & jst)
{
    // Reset all positions to 0.
    jst._activeBlock = 0;
    if (!fullJournalRequired(jst))
    {
        jst._emptyJournal = true;
        arrayFill(begin(jst._activeBlockVPOffset, Standard()), end(jst._activeBlockVPOffset, Standard()), 0);
        arrayFill(begin(jst._blockVPOffset, Standard()), end(jst._blockVPOffset, Standard()), 0);
    }
    jst._mapBlockBegin = jst._mapBlockEnd = begin(container(jst), Standard());
}

// ----------------------------------------------------------------------------
// Function init()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#init
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Initializes the journaled string tree.
 *
 * @signature void init(jst, host, delta);
 *
 * @param[in,out] jst    The Journal String Tree.
 * @param[in] host       The reference sequence.
 * @param[in] delta      The object containing the delta information.
 *
 * This function does not construct the sequences. Use the function @link JournaledStringTree#create @endlink and
 * @link JournaledStringTree#createNext @endlink to dynamically generate the sequences on-demand.
 */

template <typename TDeltaMap, typename TSpec, typename THost>
inline void
init(JournaledStringTree<TDeltaMap, TSpec> & jst,
     THost & referenceSeq,
     TDeltaMap & varData)
{
    typedef JournaledStringTree<TDeltaMap, TSpec> TJst;
    typedef typename GetStringSet<TJst>::Type TStringSet;
    typedef typename Value<TStringSet>::Type TString;

    setValue(jst._container, varData);
    setHost(stringSet(jst), referenceSeq);

    TString tmp;
    setHost(tmp, host(stringSet(jst)));
    resize(stringSet(jst), coverageSize(varData), tmp, Exact());
    resize(jst._blockVPOffset, length(stringSet(jst)), 0, Exact());
    resize(jst._activeBlockVPOffset, length(stringSet(jst)), 0, Exact());

    jst._mapBlockBegin = jst._mapBlockEnd = begin(varData, Standard());
}

// ----------------------------------------------------------------------------
// Function setBlockSize()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#setBlockSize
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Sets the number of variants processed at a time.
 *
 * @signature void setBlockSize(jst, block);
 *
 * @param[in]   jst     The Journal String Tree.
 * @param[in]   block   The number of deltas contained in one block.
 *
 * Per default all deltas are processed in a single block.
 *
 * @see JournaledStringTree#getBlockSize
 */

template <typename TDeltaMap, typename TSpec, typename TPosition>
inline void
setBlockSize(JournaledStringTree<TDeltaMap, TSpec> & stringTree,
             TPosition const & newBlockSize)
{
    SEQAN_ASSERT_NOT(empty(stringTree._container));  // The delta map needs to be set before.

    stringTree._blockSize = newBlockSize;
    stringTree._activeBlock = 0;
    stringTree._numBlocks = static_cast<unsigned>(std::ceil(static_cast<double>(length(container(stringTree))) /
                                                            static_cast<double>(newBlockSize)));
    resize(stringTree._blockVPOffset, length(stringSet(stringTree)), 0, Exact());
}

// ----------------------------------------------------------------------------
// Function getBlockSize()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#getBlockSize
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns the size of the blocks.
 *
 * @signature TSize getBlockSize(jst);
 *
 * @param[in]   jst     The Journal String Tree.
 *
 * @return TSize The size of the blocks of type @link JournaledStringTree#Size @endlink.
 *
 * @see JournaledStringTree#setBlockSize
 */

template <typename TDeltaMap, typename TSpec>
inline typename Size<JournaledStringTree<TDeltaMap, TSpec> >::Type
getBlockSize(JournaledStringTree<TDeltaMap, TSpec> const & stringTree)
{
    return stringTree._blockSize;
}

// ----------------------------------------------------------------------------
// Function container()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#container
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns a reference to the object holding the delta information for a set of sequences.
 *
 * @signature TContainer container(jst);
 *
 * @param[in]   jst     The Journal String Tree.
 *
 * @return TContainer A reference to the object holding the delta information of type @link JournaledStringTree#Container @endlink.
 *
 * @see JournaledStringTree#stringSet
 */

template <typename TDeltaMap, typename TSpec>
inline typename Container<JournaledStringTree<TDeltaMap, TSpec> >::Type &
container(JournaledStringTree<TDeltaMap, TSpec> & stringTree)
{
    return value(stringTree._container);
}

template <typename TDeltaMap, typename TSpec>
inline typename Container<JournaledStringTree<TDeltaMap, TSpec> const>::Type &
container(JournaledStringTree<TDeltaMap, TSpec> const & stringTree)
{
    return value(stringTree._container);
}

// ----------------------------------------------------------------------------
// Function stringSet()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#stringSet
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Returns a reference to the constructed sequences.
 *
 * @signature TStringSet stringSet(jst);
 *
 * @param[in]   jst     The Journal String Tree.
 *
 * @return TStringSet A reference to the object containing the compressed strings @link JournaledStringTree#GetStringSet @endlink.
 *
 * @see JournaledStringTree#container
 */

template <typename TDeltaMap, typename TSpec>
inline typename GetStringSet<JournaledStringTree<TDeltaMap, TSpec> >::Type &
stringSet(JournaledStringTree<TDeltaMap, TSpec> & stringTree)
{
    return stringTree._journalSet;
}

template <typename TDeltaMap, typename TSpec>
inline typename GetStringSet<JournaledStringTree<TDeltaMap, TSpec> const>::Type &
stringSet(JournaledStringTree<TDeltaMap, TSpec> const & stringTree)
{
    return stringTree._journalSet;
}

// ----------------------------------------------------------------------------
// Function open()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#open
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Opens the journaled string tree from a gdf file.
 *
 * @signature int load(jst, filename[, refId, refFilename][, seqIds]);
 *
 * @param[in,out] jst The journaled string tree to be loaded.
 * @param[in] filename The name of the file used to load the data from.
 * @param[out] refId Stores the id of the reference.
 * @param[out] refFilename Stores the file name of the reference.
 * @param[out] seqIds Stores the ids of the covered sequences.
 *
 * @return int <tt>0<\tt> on success, otherwise some value distinct from <tt>0<\tt> indicating the error.
 *
 * @see JournaledStringTree#save
 */

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int open(JournaledStringTree<TDeltaMap, TSpec> & jst,
                TFilename const & filename,
                CharString & refId,
                CharString & refFileName,
                String<CharString> & nameStore)
{
    typedef JournaledStringTree<TDeltaMap, TSpec> TJst;
    typedef typename Host<TJst>::Type THost;

    GdfHeader<> gdfHeader;
    gdfHeader._nameStorePtr = &nameStore;  // Check if this works.

    std::ifstream inputFile;
    inputFile.open(toCString(filename), std::ios_base::in | std::ios_base::binary);
    if (!inputFile.good())
    {
        std::cerr << "Cannot open file <" << filename << "> for reading!" << std::endl;
        return inputFile.rdstate();
    }
    int res = read(container(jst), gdfHeader, inputFile, Gdf());
    if (res != 0)
    {
        std::cerr << "An error occurred while reading <" << filename << ">!" << std::endl;
        return res;
    }
    inputFile.close();

    // Read the reference file.
    refId = gdfHeader._refInfos._refId;
    refFileName = gdfHeader._refInfos._refFile;

    std::ifstream refFile;
    refFile.open(toCString(refFileName), std::ios_base::in);
    if (!refFile.good())
    {
        std::cerr << "Cannot open file <" << refFileName << "> for reading!" << std::endl;
        return refFile.rdstate();
    }
    THost tmpHost = "";
    createHost(stringSet(jst), tmpHost);
    RecordReader<std::ifstream, SinglePass<> > reader(refFile);
    res = readRecord(refId, host(jst), reader, Fasta());
    refFile.close();
    if (res != 0)
        std::cerr << "An error occurred while reading <" << refFileName << ">!" << std::endl;

    // Initialize the journaled string tree.
    resize(stringSet(jst), coverageSize(container(jst)), host(stringSet(jst)), Exact());
    resize(jst._blockVPOffset, length(stringSet(jst)), 0, Exact());
    resize(jst._activeBlockVPOffset, length(stringSet(jst)), 0, Exact());

    jst._mapBlockBegin = jst._mapBlockEnd = begin(container(jst), Standard());

    return res;
}

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int open(JournaledStringTree<TDeltaMap, TSpec> & jst,
                TFilename const & filename,
                CharString & refId,
                CharString & refFileName)
{
    String<CharString> nameStore;
    return open(jst, filename, refId, refFileName, nameStore);
}

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int open(JournaledStringTree<TDeltaMap, TSpec> & jst,
                TFilename const & filename,
                String<CharString> & nameStore)
{
    CharString refId;
    CharString refFileName;
    return open(jst, filename, refId, refFileName, nameStore);
}


template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int open(JournaledStringTree<TDeltaMap, TSpec> & jst,
                TFilename const & filename)
{
    CharString refId;
    CharString refFileName;
    return open(jst, filename, refId, refFileName);
}

// ----------------------------------------------------------------------------
// Function save()
// ----------------------------------------------------------------------------

/*!
 * @fn JournaledStringTree#save
 * @headerfile <seqan/journaled_string_tree.h>
 * @brief Saves the journaled string tree at the given location in gdf format.
 *
 * @signature int save(jst, filename[, refId, refFilename][, seqIds]);
 *
 * @param[in] jst The journaled string tree to be saved.
 * @param[out] filename The name of the file used to save the journaled string tree.
 * @param[in] refId Id of the reference sequence.
 * @param[in] refFilename File name of the reference.
 * @param[in] seqIds Ids of the sequences covered by the jst.
 *
 * Stores the journaled string tree in gdf format at the specified file location. Please note,
 * that <tt>refId<\tt> and <tt>refFilename<\tt>, as well as <tt>seqIds<\tt> are optional parameters.
 * If they are empty, some default values are used instead. If the information regarding the reference are empty,
 * then the reference file is automatically stored in fasta format at <tt><filename.reference.fa><\tt>.
 *
 * @return int <tt>0<\tt> on success, otherwise some value distinct from <tt>0<\tt> indicating the error.
 *
 * @see JournaledStringTree#save
 */

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int save(JournaledStringTree<TDeltaMap, TSpec> const & jst,
                TFilename const & filename,
                CharString const & refId,
                CharString const & refFileName,
                String<CharString> const & nameStore)
{
    typedef typename DeltaValue<TDeltaMap, DeltaType::DELTA_TYPE_SNP>::Type TSnpType;

    GdfHeader<> gdfHeader;
    gdfHeader._refInfos._refId = refId;
    // Write reference if unknown.
    if (empty(refFileName))
    {
        gdfHeader._refInfos._refFile = filename;
        append(gdfHeader._refInfos._refFile, ".reference.fa");
        std::ofstream refStream;
        refStream.open(toCString(gdfHeader._refInfos._refFile), std::ios_base::out);
        if (!refStream.good())
        {
            std::cerr << "Cannot open file <" << gdfHeader._refInfos._refFile << "> for writing!" << std::endl;
            return refStream.rdstate();
        }
        writeRecord(refStream, gdfHeader._refInfos._refId, host(jst), Fasta());
        refStream.close();
    }
    gdfHeader._refInfos._refHash = 0;  // TODO(rmaerker): Compute hash for reference.

    // Enable snp compression if at most 2 bits are needed to store the value.
    if (BitsPerValue<TSnpType>::VALUE <= 2)
        gdfHeader._fileInfos._snpCompression = true;
    else
        gdfHeader._fileInfos._snpCompression = false;

    // We know that we don't change the names.
    gdfHeader._nameStorePtr = const_cast<String<CharString>*>(&nameStore);

    std::ofstream fileStream;
    fileStream.open(toCString(filename), std::ios_base::out | std::ios_base::binary);

    if (!fileStream.good())
    {
        std::cerr << "Cannot open file <" << filename << "> for writing!" << std::endl;
        return fileStream.rdstate();
    }
    int res = write(fileStream, container(jst), gdfHeader, Gdf());
    fileStream.close();

    if (res != 0)
        std::cerr << "An error occurred while writing!" << std::endl;
    return res;
}

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int save(JournaledStringTree<TDeltaMap, TSpec> const & jst,
                TFilename const & filename,
                String<CharString> & nameStore)
{
    return save(jst, filename, "NA", "", nameStore);
}

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int save(JournaledStringTree<TDeltaMap, TSpec> const & jst,
                TFilename const & filename,
                CharString const & refId,
                CharString const & refFileName)
{
    String<CharString> emptyIds;
    // Default naming of sequences.
    for (unsigned i = 0; i < coverageSize(container(jst)); ++i)
    {
        std::stringstream tmp;
        tmp << "seq" << i;
        appendValue(emptyIds, tmp.str());
    }
    return save(jst, filename, refId, refFileName, emptyIds);
}

template <typename TDeltaMap, typename TSpec, typename TFilename>
inline int save(JournaledStringTree<TDeltaMap, TSpec> const & jst,
                TFilename const & filename)
{
    return save(jst, filename, "NA", "");
}

}

#endif // EXTRAS_INCLUDE_SEQAN_JOURNALED_STRING_TREE_JOURNALED_STRING_TREE_IMPL_H_
