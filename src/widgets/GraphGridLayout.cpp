#include "GraphGridLayout.h"

#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <cassert>

/** @class GraphGridLayout

Basic familarity with graph algorithms is recommended.

# Terms used:
- **Vertice**, **node**, **block** - read description of graph for definition. Within this text vertice and node are
used interchangably with block due to code being written for visualizing basic block controll flow graph.
- **edge** - read description of graph for definition for precise definition.
- **DAG** - directed acyclic graph, graph using directed edges which doesn't have cycles. DAG may contain loops if
following them would require going in both directions of edges. Example 1->2 1->3 3->2 is a DAG, 2->1 1->3 3->2
isn't a DAG.
- **DFS** - depth first search, a graph traversal algorithm
- **toposort** - toplogical sorting, process of ordering a DAG vertices that all edges go from vertices erlier in the
toposort order to vertices later in toposort order. There are multiple algorithms for implementing toposort operation.
Single DAG can have multiple valid topoligical orderings, a toposort algorithm can be designed to priotarize a specific
one from all valid toposort orders. Example: for graph 1->4, 2->1, 2->3, 3->4 valid topological orders are [2,1,3,4] and
[2,3,1,4].

# High level strucutre of the algorithm
1. select subset of edges that form a DAG (remove cycles)
2. toposort the DAG
3. choose a subset of edges that form a tree and assign layers
4. assign node positions within grid using tree structure, child subtrees are placed side by side with parent on top
5. perform edge routing
6. calculate column and row pixel positions based on node sizes and amount edges between the rows


Contrary to many other layered graph drawing algorithm this implementation doesn't perform node reording to minimize
edge crossing. This simplifies implementation, and preserves original control flow structure for conditional jumps (
true jump on one side, false jump on other). Due to most of control flow being result of structured programming
constructs like if/then/else and loops, resulting layout is usually readable without node reordering within layers.


# Describtion of grid.
To simplify the layout algorithm initial steps assume that all nodes have the same size and edges are zero width.
After placing the nodes and routing the edges it is known which nodes are in in which row and column, how
many edges are between each pair of rows. Using this information positions are converted from the grid cells
to pixel coordinates. Routing 0 width edges between rows can also be interpreted as every second row and column being
reserved for edges. The row numbers in code are using first interpretation. To allow better centering of nodes one
above other each node is 2 columns wide and 1 row high.

# 1-2 Cycle removal and toposort

Cycle removal and toposort are done at the same time during single DFS traversal. In case entrypoint is part of a loop
DFS started from entrypoint. This ensures that entrypoint is at the top of resulting layout if possible. Resulting
toposort order is used in many of the following layout steps that require calculating some property of a vertice based
on child property or the other way around. Using toposort order such operations can be implemented iteration through
array in either forward or reverse direction. To prevent running out of stack memory when processing large graphs
DFS is implemented non-recursively.

# Layer assignment

Layers are assigned in toposort order from top to bottom, with nodes layer being max(predecessor.layer)+1. This ensures
that loop edges are only ones going from deeper levels to previous layers.

To further simply node placment a subset of edges is selected which forms a tree. This turns DAG drawing problem
into a tree drawing problem. For each node in level n following nodes which have level exactly n+1 are greedily
assigned as child nodes in tree. If a node already has perant assigned then corresponding edge is not part of tree.

# Node position assignment

Since the graph has been reduced to a tree node placement is more or less putting subtrees side by side with
parent on top. There is some room for interpretation what exactly side by side means and where exactly on top is.
Drawing the graph either too dense or too big may make it less readable so there are configuration options which allow
choosing these things resulting in more or less dense layout.

Current layout algorithm defines subtree size as it's bounding box and in most cases puts the bounding boxes side by
side. The layout could be made more dense by taking exact shape into account. There is a special case for ignoring
bounding box when one of 2 subtrees contain's exactly 1 vertice.

Other choice is wether to place node horizontally in the middle between direct child nodes or in the middle of
subtree width.

That results in 3 modes

- **wide** - bounding boxes are always side by side, no exception for single vertice subtree
- **medium** - use exception for single vertice subtree, place node in the middle of direct children. In case of long
 if elseif chanin produces staircase shape.
- **narrow** - use exception for single vertice subtree, place node in the middle of subtree total width. In case of
 if elseif chain produces two columns.

# Edge routing
Edge routing can be split into 3 stages. Rough routing within grid, overlaping edge prevention and converting to
pixel coordinates.

Due to nodes being placed in a grid. Horizontal segments of edges can't intersect with any nodes. The path for edges
is chosen so that it consists of at most 5 segments, typically resulting in sidedway U shape or square Z shape.
- short vertical segment from node to horizontal line
- move to empty column
- vertical segment between starting row and end row, an empty column can always be found, in the worst case there are empty columns at the sides of drawing
- horizontal segment to target node column
- short vertical segment connecting to target node

There are 3 special cases:
- source and target nodes are in the same column with no nodes betweeen - single vertical segment
- column bellow stating node is empty - segments 1-3 are merged
- column above target node is empty - segments 3-5 are merged
Vertical segment intersection with nodes is prevented using a 2d arry marking which vertical segments are blocked and
naively iterating through all rows between start and end at the desired column.

Edge overlap within a column or row is prevented by spliting columns into sub-columns. Used subcolumns are stored and
chechked using a 2d array of lists.

*/

namespace {
class MinTree1
{
public:
    MinTree1(size_t size)
        : size(size)
        , nodeCount(2 * size)
        , nodes(nodeCount)
    {
    }

    MinTree1(size_t size, int value)
        : MinTree1(size)
    {
        init(value);
    }

    void build()
    {
        for (size_t i = size - 1; i > 0; i--) {
            nodes[i] = std::min(nodes[i << 1], nodes[(i << 1) | 1]);
        }
    }
    void init(int value)
    {
        std::fill_n(nodes.begin() + size, size, value);
    }

    void set(size_t pos, int value)
    {
        nodes[pos + size] = value;
        while (pos > 1) {
            auto parrent = pos >> 1;
            nodes[parrent] = std::min(nodes[pos], nodes[pos ^ 1]);
            pos = parrent;
        }
    }
    int valueAtPoint(size_t pos)
    {
        return nodes[positionToLeaveIndex(pos)];
    }
    size_t leaveIndexToPosition(size_t index)
    {
        return index - size;
    }

    size_t positionToLeaveIndex(size_t position)
    {
        return position + size;
    }

    /**
     * @brief Find right most position with value than less than given in range [0; position].
     * @param position inclusive right side of query range
     * @param value search for position with value less than this
     * @return returns the position with searched property or -1 if there is no such position.
     */
    int rightMostLessThan(size_t position, int value)
    {
        // right side exclusive range [l;r)
        size_t goodSubtree = 0;
        for (size_t l = positionToLeaveIndex(0), r = positionToLeaveIndex(position + 1); l < r;
                l >>= 1, r >>= 1) {
            if (l & 1) {
                if (nodes[l] < value) {
                    // mark subtree as good but don't stop yet, there might be something good further to the right
                    goodSubtree = l;
                }
                ++l;
            }
            if (r & 1) {
                --r;
                if (nodes[r] <  value) {
                    goodSubtree = r;
                    break;
                }
            }
        }
        if (!goodSubtree) {
            return -1;
        }
        // find rightmost branch of subtree
        while (goodSubtree < size) {
            goodSubtree = (goodSubtree << 1) + 1;
        }
        return goodSubtree - size; // convert from node index to position in range (0;size]
    }

    /**
     * @brief Find left most position with value than less than given in range [position; size).
     * @param position inclusive left side of query range
     * @param value search for position with value less than this
     * @return returns the position with searched property or -1 if there is no such position.
     */
    int leftMostLessThan(size_t position, int value)
    {
        // right side exclusive range [l;r)
        size_t goodSubtree = 0;
        for (size_t l = positionToLeaveIndex(position), r = positionToLeaveIndex(size); l < r;
                l >>= 1, r >>= 1) {
            if (l & 1) {
                if (nodes[l] < value) {
                    goodSubtree = l;
                    break;
                }
                ++l;
            }
            if (r & 1) {
                --r;
                if (nodes[r] <  value) {
                    goodSubtree = r;
                    // mark subtree as good but don't stop yet, there might be something good further to the left
                }
            }
        }
        if (!goodSubtree) {
            return -1;
        }
        // find rightmost branch of subtree
        while (goodSubtree < size) {
            goodSubtree = (goodSubtree << 1);
        }
        return goodSubtree - size; // convert from node index to position in range (0;size]
    }
private:
    const size_t size; //< number of leaves and also index of left most leave
    const size_t nodeCount;
    std::vector<int> nodes;
};

}

GraphGridLayout::GraphGridLayout(GraphGridLayout::LayoutType layoutType)
    : GraphLayout({})
, layoutType(layoutType)
{
}

std::vector<ut64> GraphGridLayout::topoSort(LayoutState &state, ut64 entry)
{
    auto &blocks = *state.blocks;

    // Run DFS to:
    // * select backwards/loop edges
    // * perform toposort
    std::vector<ut64> blockOrder;
    // 0 - not visited
    // 1 - in stack
    // 2 - visited
    std::unordered_map<ut64, uint8_t> visited;
    visited.reserve(state.blocks->size());
    std::stack<std::pair<ut64, size_t>> stack;
    auto dfsFragment = [&visited, &blocks, &state, &stack, &blockOrder](ut64 first) {
        visited[first] = 1;
        stack.push({first, 0});
        while (!stack.empty()) {
            auto v = stack.top().first;
            auto edge_index = stack.top().second;
            const auto &block = blocks[v];
            if (edge_index < block.edges.size()) {
                ++stack.top().second;
                auto target = block.edges[edge_index].target;
                auto &targetState = visited[target];
                if (targetState == 0) {
                    targetState = 1;
                    stack.push({target, 0});
                    state.grid_blocks[v].dag_edge.push_back(target);
                } else if (targetState == 2) {
                    state.grid_blocks[v].dag_edge.push_back(target);
                } // else {  targetState == 1 in stack, loop edge }
            } else {
                stack.pop();
                visited[v] = 2;
                blockOrder.push_back(v);
            }
        }
    };

    // Start with entry so that if start of function block is part of loop it
    // is still kept at top unless it's impossible to do while maintaining
    // topological order.
    dfsFragment(entry);
    for (auto &blockIt : blocks) {
        if (!visited[blockIt.first]) {
            dfsFragment(blockIt.first);
        }
    }

    // assign layers and select tree edges
    for (auto it = blockOrder.rbegin(), end = blockOrder.rend(); it != end; it++) {
        auto &block = state.grid_blocks[*it];
        int nextLevel = block.level + 1;
        for (auto target : block.dag_edge) {
            auto &targetBlock = state.grid_blocks[target];
            targetBlock.level = std::max(targetBlock.level, nextLevel);
        }
    }
    for (auto &blockIt : state.grid_blocks) {
        auto &block = blockIt.second;
        for (auto targetId : block.dag_edge) {
            auto &targetBlock = state.grid_blocks[targetId];
            if (!targetBlock.has_parent && targetBlock.level == block.level + 1) {
                block.tree_edge.push_back(targetId);
                targetBlock.has_parent = true;
            }
        }
    }
    return blockOrder;
}

void GraphGridLayout::CalculateLayout(std::unordered_map<ut64, GraphBlock> &blocks, ut64 entry,
                                      int &width, int &height) const
{
    LayoutState layoutState;
    layoutState.blocks = &blocks;

    for (auto &it : blocks) {
        GridBlock block;
        block.id = it.first;
        layoutState.grid_blocks[it.first] = block;
    }

    auto block_order = topoSort(layoutState, entry);
    computeAllBlockPlacement(block_order, layoutState);

    for (auto &blockIt : blocks) {
        layoutState.edge[blockIt.first].resize(blockIt.second.edges.size());
    }

    // Prepare edge routing
    int col_count = 1;
    int row_count = 0;
    for (const auto &blockIt : layoutState.grid_blocks) {
        if (!blockIt.second.has_parent) {
            row_count = std::max(row_count, blockIt.second.row_count);
            col_count += blockIt.second.col_count;
        }
    }
    row_count += 2;
    EdgesVector horiz_edges, vert_edges;
    horiz_edges.resize(row_count + 1);
    vert_edges.resize(row_count + 1);
    Matrix<bool> edge_valid;
    edge_valid.resize(row_count + 1);
    for (int row = 0; row < row_count + 1; row++) {
        horiz_edges[row].resize(col_count + 1);
        vert_edges[row].resize(col_count + 1);
        edge_valid[row].assign(col_count + 1, true);
    }

    for (auto &blockIt : layoutState.grid_blocks) {
        auto &block = blockIt.second;
        edge_valid[block.row][block.col + 1] = false;
    }

    // Perform edge routing
    for (ut64 blockId : block_order) {
        GraphBlock &block = blocks[blockId];
        GridBlock &start = layoutState.grid_blocks[blockId];
        size_t i = 0;
        for (const auto &edge : block.edges) {
            GridBlock &end = layoutState.grid_blocks[edge.target];
            layoutState.edge[blockId][i++] = routeEdge(horiz_edges, vert_edges, edge_valid, start, end);
        }
    }

    // Compute edge counts for each row and column
    std::vector<int> col_edge_count, row_edge_count;
    col_edge_count.assign(col_count + 1, 0);
    row_edge_count.assign(row_count + 1, 0);
    for (int row = 0; row < row_count + 1; row++) {
        for (int col = 0; col < col_count + 1; col++) {
            if (int(horiz_edges[row][col].size()) > row_edge_count[row])
                row_edge_count[row] = int(horiz_edges[row][col].size());
            if (int(vert_edges[row][col].size()) > col_edge_count[col])
                col_edge_count[col] = int(vert_edges[row][col].size());
        }
    }


    //Compute row and column sizes
    std::vector<int> col_width, row_height;
    col_width.assign(col_count + 1, 0);
    row_height.assign(row_count + 1, 0);
    for (auto &blockIt : blocks) {
        GraphBlock &block = blockIt.second;
        GridBlock &grid_block = layoutState.grid_blocks[blockIt.first];
        if ((int(block.width / 2)) > col_width[grid_block.col])
            col_width[grid_block.col] = int(block.width / 2);
        if ((int(block.width / 2)) > col_width[grid_block.col + 1])
            col_width[grid_block.col + 1] = int(block.width / 2);
        if (int(block.height) > row_height[grid_block.row])
            row_height[grid_block.row] = int(block.height);
    }

    // Compute row and column positions
    std::vector<int> col_x, row_y;
    col_x.assign(col_count, 0);
    row_y.assign(row_count, 0);
    std::vector<int> col_edge_x(col_count + 1);
    std::vector<int> row_edge_y(row_count + 1);
    int x = layoutConfig.block_horizontal_margin;
    for (int i = 0; i <= col_count; i++) {
        col_edge_x[i] = x;
        x += layoutConfig.block_horizontal_margin * col_edge_count[i];
        if (i < col_count) {
            col_x[i] = x;
            x += col_width[i];
        }
    }
    int y = layoutConfig.block_vertical_margin;
    for (int i = 0; i <= row_count; i++) {
        row_edge_y[i] = y;
        if (!row_edge_count[i]) {
            // prevent 2 blocks being put on top of each other without any space
            row_edge_count[i] = 1;
        }
        y += layoutConfig.block_vertical_margin * row_edge_count[i];
        if (i < row_count) {
            row_y[i] = y;
            y += row_height[i];
        }
    }
    width = x + (layoutConfig.block_horizontal_margin);
    height = y + (layoutConfig.block_vertical_margin);

    //Compute node positions
    for (auto &blockIt : blocks) {
        GraphBlock &block = blockIt.second;
        GridBlock &grid_block = layoutState.grid_blocks[blockIt.first];
        auto column = grid_block.col;
        auto row = grid_block.row;
        block.x = int(col_x[column] + col_width[column] +
                      ((layoutConfig.block_horizontal_margin / 2) * col_edge_count[column + 1])
                      - (block.width / 2));
        if ((block.x + block.width) > (
                    col_x[column] + col_width[column] + col_width[column + 1] +
                    layoutConfig.block_horizontal_margin *
                    col_edge_count[column + 1])) {
            block.x = int((col_x[column] + col_width[column] + col_width[column + 1] +
                           layoutConfig.block_horizontal_margin * col_edge_count[column + 1]) - block.width);
        }
        block.y = row_y[row];
    }

    // Compute coordinates for edges
    auto position_from_middle = [](int index, int spacing, int column_count) {
        return spacing * (((index & 1) ? 1 : -1) * ((index + 1) / 2) + (column_count - 1) / 2);
    };
    for (auto &blockIt : blocks) {
        GraphBlock &block = blockIt.second;

        size_t index = 0;
        assert(block.edges.size() == layoutState.edge[block.entry].size());
        for (GridEdge &edge : layoutState.edge[block.entry]) {
            if (edge.points.empty()) {
                qDebug() << "Warning, unrouted edge.";
                continue;
            }
            auto start = edge.points[0];
            auto start_col = start.col;
            auto last_index = edge.start_index;
            // This is the start point of the edge.
            auto first_pt = QPoint(col_edge_x[start_col] +
                                   position_from_middle(last_index, layoutConfig.block_horizontal_margin, col_edge_count[start_col]) +
                                   (layoutConfig.block_horizontal_margin / 2),
                                   block.y + block.height);
            auto last_pt = first_pt;
            QPolygonF pts;
            pts.append(last_pt);

            for (int i = 0; i < int(edge.points.size()); i++) {
                auto end = edge.points[i];
                auto end_row = end.row;
                auto end_col = end.col;
                auto last_index = end.index;
                QPoint new_pt;
                // block_vertical_margin/2 gives the margin from block to the horizontal lines
                if (start_col == end_col)
                    new_pt = QPoint(last_pt.x(), row_edge_y[end_row] +
                                    position_from_middle(last_index, layoutConfig.block_vertical_margin, row_edge_count[end_row]) +
                                    (layoutConfig.block_vertical_margin / 2));
                else
                    new_pt = QPoint(col_edge_x[end_col] +
                                    position_from_middle(last_index, layoutConfig.block_horizontal_margin, col_edge_count[end_col]) +
                                    (layoutConfig.block_horizontal_margin / 2), last_pt.y());
                pts.push_back(new_pt);
                last_pt = new_pt;
                start_col = end_col;
            }

            const auto &target = blocks[edge.dest];
            auto new_pt = QPoint(last_pt.x(), target.y - 1);
            pts.push_back(new_pt);
            block.edges[index].polyline = pts;
            index++;
        }
    }
}

void GraphGridLayout::computeAllBlockPlacement(const std::vector<ut64> &blockOrder,
                                               LayoutState &layoutState) const
{
    for (auto blockId : blockOrder) {
        computeBlockPlacement(blockId, layoutState);
    }
    int col = 0;
    for (auto blockId : blockOrder) {
        if (!layoutState.grid_blocks[blockId].has_parent) {
            adjustGraphLayout(layoutState.grid_blocks[blockId], layoutState.grid_blocks, col, 1);
            col += layoutState.grid_blocks[blockId].col_count;
        }
    }
}

// Prepare graph
// This computes the position and (row/col based) size of the block
void GraphGridLayout::computeBlockPlacement(ut64 blockId, LayoutState &layoutState) const
{
    auto &block = layoutState.grid_blocks[blockId];
    auto &blocks = layoutState.grid_blocks;
    int col = 0;
    int row_count = 1;
    int childColumn = 0;
    bool singleChild = block.tree_edge.size() == 1;
    // Compute all children nodes
    for (size_t i = 0; i < block.tree_edge.size(); i++) {
        ut64 edge = block.tree_edge[i];
        auto &edgeb = blocks[edge];
        row_count = std::max(edgeb.row_count + 1, row_count);
        childColumn = edgeb.col;
    }

    if (layoutType != LayoutType::Wide && block.tree_edge.size() == 2) {
        auto &left = blocks[block.tree_edge[0]];
        auto &right = blocks[block.tree_edge[1]];
        if (left.tree_edge.size() == 0) {
            left.col = right.col - 2;
            int add = left.col < 0 ? - left.col : 0;
            adjustGraphLayout(right, blocks, add, 1);
            adjustGraphLayout(left, blocks, add, 1);
            col = right.col_count + add;
        } else if (right.tree_edge.size() == 0) {
            adjustGraphLayout(left, blocks, 0, 1);
            adjustGraphLayout(right, blocks, left.col + 2, 1);
            col = std::max(left.col_count, right.col + 2);
        } else {
            adjustGraphLayout(left, blocks, 0, 1);
            adjustGraphLayout(right, blocks, left.col_count, 1);
            col = left.col_count + right.col_count;
        }
        block.col_count = std::max(2, col);
        if (layoutType == LayoutType::Medium) {
            block.col = (left.col + right.col) / 2;
        } else {
            block.col = singleChild ? childColumn : (col - 2) / 2;
        }
    } else {
        for (ut64 edge : block.tree_edge) {
            adjustGraphLayout(blocks[edge], blocks, col, 1);
            col += blocks[edge].col_count;
        }
        if (col >= 2) {
            // Place this node centered over the child nodes
            block.col = singleChild ? childColumn : (col - 2) / 2;
            block.col_count = col;
        } else {
            //No child nodes, set single node's width (nodes are 2 columns wide to allow
            //centering over a branch)
            block.col = 0;
            block.col_count = 2;
        }
    }
    block.row = 0;
    block.row_count = row_count;
}

void GraphGridLayout::adjustGraphLayout(GridBlock &block,
                                        std::unordered_map<ut64, GridBlock> &blocks, int col, int row) const
{
    block.col += col;
    block.row += row;
    for (ut64 edge : block.tree_edge) {
        adjustGraphLayout(blocks[edge], blocks, col, row);
    }
}

// Edge computing stuff
bool GraphGridLayout::isEdgeMarked(EdgesVector &edges, int row, int col, int index)
{
    if (index >= int(edges[row][col].size()))
        return false;
    return edges[row][col][index];
}

void GraphGridLayout::markEdge(EdgesVector &edges, int row, int col, int index, bool used)
{
    while (int(edges[row][col].size()) <= index)
        edges[row][col].push_back(false);
    edges[row][col][index] = used;
}

void GraphGridLayout::routeEdges(GraphGridLayout::LayoutState &state) const
{
    //first part route the edges within grid, avoiding nodes
    // Use sweep line approach processing events sorted by row
    size_t columns = 1;
    size_t rows = 1;
    for (auto &node : state.grid_blocks) {
        // block is 2 column wide so index for edge column to the right of block is
        // +2, and edge column count is at least 2 + 1
        columns = std::max(columns, size_t(node.second.col) + 3);
        rows = std::max(rows, size_t(node.second.row) + 1);
    }
    struct Event {
        size_t blockId;
        size_t edgeId;
        int row;
        enum Type {
            Edge = 0,
            Block = 1
        } type;
    };
    // create events
    std::vector<Event> events;
    events.reserve(state.grid_blocks.size() * 2);
    for (const auto &it : state.grid_blocks) {
        events.push_back({it.first, 0, it.second.row, Event::Block});
        const auto &inputBlock = (*state.blocks)[it.first];
        int startRow = it.second.row + 1;

        auto gridEdges = state.edge[it.first];
        gridEdges.resize(inputBlock.edges.size());
        for (size_t i = 0; i < inputBlock.edges.size(); i++) {
            auto targetId = inputBlock.edges[i].target;
            gridEdges[i].dest = targetId;
            const auto &targetGridBlock = state.grid_blocks[targetId];
            int endRow = targetGridBlock.row;
            Event e{targetId, i, std::max(startRow, endRow), Event::Edge};
            events.push_back(e);
        }
    }
    std::sort(events.begin(), events.end(),
        [](const Event & a, const Event & b) {
        if (a.row != b.row) {
            return a.row < b.row;
        }
        return static_cast<int>(a.type) < static_cast<int>(b.type);
    });

    // process events and choose main column for each edge
    MinTree1 blockedColumns(columns, -1);
    for (const auto &event : events) {
        if (event.type == Event::Block) {
            auto block = state.grid_blocks[event.blockId];
            blockedColumns.set(block.col + 1, event.row);
        } else {
            auto block = state.grid_blocks[event.blockId];
            int column = block.col + 1;
            auto &edge = state.edge[event.blockId][event.edgeId];
            const auto &targetBlock = state.grid_blocks[edge.dest];
            auto topRow = std::min(block.row + 1, targetBlock.row);

            // Prefer using the same column as starting node or target node.
            // It allows reducing amount of segments.
            if (blockedColumns.valueAtPoint(column) < topRow) {
                edge.mainColumn = column;
            } else if (blockedColumns.valueAtPoint(targetBlock.col + 1) < topRow) {
                edge.mainColumn = targetBlock.col + 1;
            } else {
                auto nearestLeft = blockedColumns.rightMostLessThan(column, topRow);
                auto nearestRight = blockedColumns.leftMostLessThan(column, topRow);
                // There should always be empty column at the sides of drawing
                assert(nearestLeft != -1 && nearestRight != -1);

                // choose column closest to the middle
                if (column - nearestLeft < nearestRight - column) {
                    edge.mainColumn = nearestLeft;
                } else {
                    edge.mainColumn = nearestRight;
                }
            }
        }
    }
}


GraphGridLayout::GridEdge GraphGridLayout::routeEdge(EdgesVector &horiz_edges,
                                                     EdgesVector &vert_edges,
                                                     Matrix<bool> &edge_valid, GridBlock &start, GridBlock &end) const
{
    GridEdge edge;
    edge.dest = end.id;

    //Find edge index for initial outgoing line
    int i = 0;
    while (isEdgeMarked(vert_edges, start.row + 1, start.col + 1, i)) {
        i += 1;
    }
    markEdge(vert_edges, start.row + 1, start.col + 1, i);
    edge.addPoint(start.row + 1, start.col + 1);
    edge.start_index = i;
    bool horiz = false;

    //Find valid column for moving vertically to the target node
    int min_row, max_row;
    if (end.row < (start.row + 1)) {
        min_row = end.row;
        max_row = start.row + 1;
    } else {
        min_row = start.row + 1;
        max_row = end.row;
    }
    int col = start.col + 1;
    if (min_row != max_row) {
        auto checkColumn = [min_row, max_row, &edge_valid](int column) {
            if (column < 0 || column >= int(edge_valid[min_row].size()))
                return false;
            for (int row = min_row; row < max_row; row++) {
                if (!edge_valid[row][column]) {
                    return false;
                }
            }
            return true;
        };

        if (!checkColumn(col)) {
            if (checkColumn(end.col + 1)) {
                col = end.col + 1;
            } else {
                int ofs = 0;
                while (true) {
                    col = start.col + 1 - ofs;
                    if (checkColumn(col)) {
                        break;
                    }

                    col = start.col + 1 + ofs;
                    if (checkColumn(col)) {
                        break;
                    }

                    ofs += 1;
                }
            }
        }
    }

    if (col != (start.col + 1)) {
        //Not in same column, need to generate a line for moving to the correct column
        int min_col, max_col;
        if (col < (start.col + 1)) {
            min_col = col;
            max_col = start.col + 1;
        } else {
            min_col = start.col + 1;
            max_col = col;
        }
        int index = findHorizEdgeIndex(horiz_edges, start.row + 1, min_col, max_col);
        edge.addPoint(start.row + 1, col, index);
        horiz = true;
    }

    if (end.row != (start.row + 1)) {
        //Not in same row, need to generate a line for moving to the correct row
        if (col == (start.col + 1))
            markEdge(vert_edges, start.row + 1, start.col + 1, i, false);
        int index = findVertEdgeIndex(vert_edges, col, min_row, max_row);
        if (col == (start.col + 1))
            edge.start_index = index;
        edge.addPoint(end.row, col, index);
        horiz = false;
    }

    if (col != (end.col + 1)) {
        //Not in ending column, need to generate a line for moving to the correct column
        int min_col, max_col;
        if (col < (end.col + 1)) {
            min_col = col;
            max_col = end.col + 1;
        } else {
            min_col = end.col + 1;
            max_col = col;
        }
        int index = findHorizEdgeIndex(horiz_edges, end.row, min_col, max_col);
        edge.addPoint(end.row, end.col + 1, index);
        horiz = true;
    }

    //If last line was horizontal, choose the ending edge index for the incoming edge
    if (horiz) {
        int index = findVertEdgeIndex(vert_edges, end.col + 1, end.row, end.row);
        edge.points[int(edge.points.size()) - 1].index = index;
    }

    return edge;
}


int GraphGridLayout::findHorizEdgeIndex(EdgesVector &edges, int row, int min_col, int max_col)
{
    //Find a valid index
    int i = 0;
    while (true) {
        bool valid = true;
        for (int col = min_col; col < max_col + 1; col++)
            if (isEdgeMarked(edges, row, col, i)) {
                valid = false;
                break;
            }
        if (valid)
            break;
        i++;
    }

    //Mark chosen index as used
    for (int col = min_col; col < max_col + 1; col++)
        markEdge(edges, row, col, i);
    return i;
}

int GraphGridLayout::findVertEdgeIndex(EdgesVector &edges, int col, int min_row, int max_row)
{
    //Find a valid index
    int i = 0;
    while (true) {
        bool valid = true;
        for (int row = min_row; row < max_row + 1; row++)
            if (isEdgeMarked(edges, row, col, i)) {
                valid = false;
                break;
            }
        if (valid)
            break;
        i++;
    }

    //Mark chosen index as used
    for (int row = min_row; row < max_row + 1; row++)
        markEdge(edges, row, col, i);
    return i;
}
