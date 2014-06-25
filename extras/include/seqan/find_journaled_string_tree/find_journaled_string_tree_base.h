// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2013, Knut Reinert, FU Berlin
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
// Tags and structures used globally for journaled string tree finder.
// ==========================================================================

#ifndef EXTRAS_INCLUDE_SEQAN_FIND_JOURNALED_STRING_TREE_FIND_JOURNALED_STRING_TREE_BASE_H_
#define EXTRAS_INCLUDE_SEQAN_FIND_JOURNALED_STRING_TREE_FIND_JOURNALED_STRING_TREE_BASE_H_

namespace seqan {

// ============================================================================
// Forwards
// ============================================================================

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

// ----------------------------------------------------------------------------
// Tag BitAlgorithmLongNeedle
// ----------------------------------------------------------------------------

struct BitAlgorithmLongNeedle_;
typedef Tag<BitAlgorithmLongNeedle_> BitAlgorithmLongNeedle;

// ----------------------------------------------------------------------------
// Tag BitAlgorithmSmallNeedle
// ----------------------------------------------------------------------------

struct BitAlgorithmSmallNeedle_;
typedef Tag<BitAlgorithmSmallNeedle_> BitAlgorithmSmallNeedle;

// ----------------------------------------------------------------------------
// Tag Jst
// ----------------------------------------------------------------------------

template <typename TSpec = void>
struct Jst;

typedef Jst<void> JstFinder;

// ----------------------------------------------------------------------------
// Class FinderExtensionPoint
// ----------------------------------------------------------------------------

/*!
 * @class FinderExtensionPoint
 * @headerfile <seqan/find_journaled_string_tree.h>
 * @implements JstFinderExtensionConcept
 * @brief A generic class to implement algorithms that can be plugged into the @link JstFinder @endlink.
 *
 * The @link JstFinder @endlink can be extended with a customized algorithm by overloading this class
 * with the desired algorithm. This class implements the @link JstFinderExtensionConcept @endlink such that
 * it can be registered to the @link JstFinder @endlink.
 *
 * @signature template <typename TFinder, typename TSpec>
 *            class FinderExtensionPoint<TFinder, TSpec>
 *
 * @tparam  TFinder The type of the finder this extension point is registered too. Must be of type @link JstFinder @endlink.
 * @tparam  TSpec   A tag specializing the algorithm to be executed.
 */

/*!
 * @fn FinderExtensionPoint::FinderExtensionPoint
 * @headerfile <seqan/find_journaled_string_tree.h>
 * @brief constructor.
 *
 * @signature FinderExtensionPoint();
 * @signature FinderExtensionPoint(obj[, limit]);
 *
 * @parm[in]    obj     The object to initialize the @link FinderExtensionPoint @endlink with.
 * @param[in]   limit   The score limit. Has to be less or equal to <tt>0</tt>.
 *
 * @see JstFinderExtensionConcept#init
 */

/*!
 * @fn FinderExtensionPoint::operator()
 * @headerfile <seqan/find_journaled_string_tree.h>
 * @brief The function call operator is called by the @link JstFinderExtensionConcept#execute @endlink interface.
 *
 * When overloading this function, the @link JstFinderExtensionConcept#execute @endlink interface needs to be
 * overloaded as well.
 *
 * @signature operator()(res, it);
 *
 * @param[in,out]   res The result of type @link Pair @endlink.
 *                      See @link JstFinderExtensionConcept#execute @endlink for more information.
 * @param[in]       it  The iterator to the current context.
 */

template <typename TFinder, typename TSpec>
class FinderExtensionPoint;

template <typename TFinder, typename TSpec>
SEQAN_CONCEPT_IMPL((JstFinderExtensionConcept), FinderExtensionPoint<TFinder, TSpec>);
template <typename TFinder, typename TSpec>
SEQAN_CONCEPT_IMPL((JstFinderExtensionConcept), FinderExtensionPoint<TFinder, TSpec> const);

// ----------------------------------------------------------------------------
// Class FinderState
// ----------------------------------------------------------------------------

template <typename TSpec = Nothing>
class FinderState;

template <typename TPattern_>
class FinderExtensionPointBase
{
public:
    typedef typename Host<TPattern>::Type   THost;
    typedef typename Size<THost>::Type      TSize;

    TPattern_& _pattern;
    TSize      _contextSize;
    bool       _isInit;

    FinderExtensionPointBase(TPattern_ & pattern) : _pattern(pattern), _contextSize(0), _isInit(true)
    {
        _contextSize = length(host(pattern));  // Default case for most simple pattern.
    }
};

// ============================================================================
// Metafunctions
// ============================================================================

// ----------------------------------------------------------------------------
// Metafunction GetPattern
// ----------------------------------------------------------------------------

// TODO(rmaerker): Can probably go to finder2_base.h in find module.
template <typename T>
struct GetPattern;

template <typename TContainer, typename TPattern, typename TSpec>
struct GetPattern<Finder_<TContainer, TPattern, TSpec> >
{
    typedef TPattern Type;
};

// ----------------------------------------------------------------------------
// Metafunction RegisteredExtensionPoint
// ----------------------------------------------------------------------------

template <typename TAlgorithm>
struct RegisteredExtensionPoint{};

template <typename TAlgorithm>
struct RegisteredExtensionPoint<TAlgorithm const> :
    RegisteredExtensionPoint<TAlgorithm>{};

// ----------------------------------------------------------------------------
// Metafunction ExtensionRegistry_
// ----------------------------------------------------------------------------

template <typename TExtensionPoint>
struct ExtensionRegistry_;

template <typename TExtensionRegistry, typename TSpec>
struct ExtensionRegistry_<FinderExtensionPoint<TExtensionRegistry, TSpec> >
{
    typedef TExtensionRegistry Type;
};

template <typename TExtensionRegistry, typename TSpec>
struct ExtensionRegistry_<FinderExtensionPoint<TExtensionRegistry, TSpec> const>
{
    typedef TExtensionRegistry const Type;
};

// ----------------------------------------------------------------------------
// Metafunction GetTraverserForFinder_
// ----------------------------------------------------------------------------

template <typename TFinder>
struct GetJstTraverserForFinder_;

// ============================================================================
// Functions
// ============================================================================

// ----------------------------------------------------------------------------
// Function isInit()
// ----------------------------------------------------------------------------

template <typename TPattern_>
inline bool
isInit(FinderExtensionPointBase<TPattern_> const & extensionBase)
{
    return extensionBase._isInit;
}

// ----------------------------------------------------------------------------
// Function setInit()
// ----------------------------------------------------------------------------

template <typename TPattern_>
inline void
setInit(FinderExtensionPointBase<TPattern_> & extensionBase)
{
    extensionBase._isInit = true;
}

// ----------------------------------------------------------------------------
// Function requireInit()
// ----------------------------------------------------------------------------

template <typename TPattern_>
inline void
requireInit(FinderExtensionPointBase<TPattern_> & extensionBase)
{
    extensionBase._isInit = false;
}

// ----------------------------------------------------------------------------
// Function setInit()
// ----------------------------------------------------------------------------

template <typename TPattern_>
inline TPattern_ &
getPattern(FinderExtensionPointBase<TPattern_> & extensionBase)
{
    return extensionBase._pattern;
}

template <typename TPattern_>
inline TPattern_ const &
getPattern(FinderExtensionPointBase<TPattern_> const & extensionBase)
{
    return extensionBase._pattern;
}

// ----------------------------------------------------------------------------
// Function contextSize()
// ----------------------------------------------------------------------------

template <typename TPattern_>
inline unsigned
contextSize(FinderExtensionPointBase<TPattern_> const & extensionBase)
{
    return extensionBase._contextSize;
}

// ----------------------------------------------------------------------------
// Function setContextSize()
// ----------------------------------------------------------------------------

template <typename TPattern_, typename TSize>
inline void
setContextSize(FinderExtensionPointBase<TPattern_> & extensionBase, TSize newSize)
{
    extensionBase._contextSize = newSize;
}

}  // namespace seqan

#endif  // EXTRAS_INCLUDE_SEQAN_FIND_JOURNALED_STRING_TREE_FIND_JOURNALED_STRING_TREE_BASE_H_
