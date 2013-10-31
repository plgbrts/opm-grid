#include <config.h>

#if HAVE_DYNAMIC_BOOST_TEST
#define BOOST_TEST_DYN_LINK
#endif
#define BOOST_TEST_MODULE PartitionIteratorCpGridTests
#include <boost/test/unit_test.hpp>

#include <dune/grid/CpGrid.hpp>
#include <dune/grid/common/gridenums.hh>
#include <dune/geometry/referenceelements.hh>
#include <dune/common/fvector.hh>

template<int codim>
void testPartitionIteratorsBasic(const Dune::CpGrid& grid)
{
    BOOST_REQUIRE((grid.leafbegin<codim,Dune::Overlap_Partition>()==
                   grid.leafend<codim,Dune::Overlap_Partition>()));
    BOOST_REQUIRE((grid.leafbegin<codim,Dune::OverlapFront_Partition>()==
                      grid.leafend<codim,Dune::OverlapFront_Partition>()));
    BOOST_REQUIRE((grid.leafbegin<codim,Dune::Ghost_Partition>()==
                   grid.leafend<codim,Dune::Ghost_Partition>()));
        // This is supposed to be a grid with only interior entities,
        // therefore the iterators for interior, interiorborder and all should match!
    BOOST_REQUIRE((grid.leafbegin<codim,Dune::Interior_Partition>()==
                   grid.leafbegin<codim,Dune::InteriorBorder_Partition>()));
    BOOST_REQUIRE((grid.leafbegin<codim,Dune::Interior_Partition>()==
                   grid.leafbegin<codim,Dune::All_Partition>()));
}

template<int codim>
void testPartitionIteratorsOnSequentialGrid(const Dune::CpGrid& grid)
{
    typename Dune::CpGrid::Traits::template Codim<codim>::
        template Partition<Dune::Interior_Partition>::LeafIterator iit=grid.leafbegin<codim,Dune::Interior_Partition>();
    typename Dune::CpGrid::Traits::template Codim<codim>::
        template Partition<Dune::InteriorBorder_Partition>::LeafIterator ibit=grid.leafbegin<codim,Dune::InteriorBorder_Partition>();
    typename Dune::CpGrid::Traits::template Codim<codim>::
        template Partition<Dune::All_Partition>::LeafIterator ait=grid.leafbegin<codim,Dune::All_Partition>();
    while(iit!=grid.leafend<codim,Dune::Interior_Partition>())
    {
        BOOST_REQUIRE((*iit==*ibit));
        BOOST_REQUIRE((*iit==*ait));
        ++iit;
        ++ibit;
        ++ait;
    }
    BOOST_REQUIRE((ibit==grid.leafend<codim,Dune::InteriorBorder_Partition>()));
    BOOST_REQUIRE((ait==grid.leafend<codim,Dune::All_Partition>()));
}


BOOST_AUTO_TEST_CASE(partitionIteratorTest)
{
    int m_argc = boost::unit_test::framework::master_test_suite().argc;
    char** m_argv = boost::unit_test::framework::master_test_suite().argv;
    Dune::MPIHelper& helper = Dune::MPIHelper::instance(m_argc, m_argv);

    if(helper.rank()==0)
    {
        Dune::CpGrid grid;
        std::array<int, 3> dims={{2, 2, 2}};
        std::array<double, 3> size={{ 1.0, 1.0, 1.0}};
        
        grid.createCartesian(dims, size);
        testPartitionIteratorsBasic<0>(grid);
        testPartitionIteratorsOnSequentialGrid<0>(grid);
        testPartitionIteratorsBasic<1>(grid);
        testPartitionIteratorsOnSequentialGrid<1>(grid);
        testPartitionIteratorsBasic<3>(grid);
        testPartitionIteratorsOnSequentialGrid<3>(grid);
    }

    if(helper.size()==1)
    {
        Dune::CpGrid grid;
        std::array<int, 3> dims={{10, 10, 10}};
        std::array<double, 3> size={{ 1.0, 1.0, 1.0}};
        
        grid.createCartesian(dims, size);
        grid.scatterGrid();
        testPartitionIteratorsBasic<0>(grid);
        testPartitionIteratorsOnSequentialGrid<0>(grid);
        testPartitionIteratorsBasic<1>(grid);
        testPartitionIteratorsOnSequentialGrid<1>(grid);
        testPartitionIteratorsBasic<3>(grid);
        testPartitionIteratorsOnSequentialGrid<3>(grid);
    }
}
