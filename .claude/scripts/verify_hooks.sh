#!/bin/bash
# Hook Setup Verification Script
# Run this to verify your hooks are properly configured

echo "=== Claude Code Hook Setup Verification ==="
echo ""

# Check if settings files exist
echo "Checking hook configuration files..."
if [ -f "/mnt/c/Devops/Projects/Polair/.claude/settings.json" ]; then
    echo "✅ Project hooks found: .claude/settings.json"
else
    echo "❌ Missing .claude/settings.json"
fi

if [ -f "/mnt/c/Devops/Projects/Polair/.claude/settings.local.json" ]; then
    echo "✅ Local hooks found: .claude/settings.local.json"
else
    echo "⚠️  Optional: .claude/settings.local.json not found"
fi

# Check commands directory
echo ""
echo "Checking slash commands..."
if [ -d "/mnt/c/Devops/Projects/Polair/.claude/commands" ]; then
    echo "✅ Commands directory exists"
    echo "Available commands:"
    ls -1 /mnt/c/Devops/Projects/Polair/.claude/commands/*.md 2>/dev/null | xargs -I {} basename {} .md | sed 's/^/  - \//'
else
    echo "❌ Commands directory not found"
fi

# Create required directories
echo ""
echo "Creating required directories..."
mkdir -p /mnt/c/Devops/Projects/Polair/.claude/logs/build
mkdir -p /mnt/c/Devops/Projects/Polair/.claude/logs/validation
mkdir -p /mnt/c/Devops/Projects/Polair/.backups
echo "✅ Log directories ready"

# Test WSL to Windows command execution
echo ""
echo "Testing WSL to Windows command bridge..."
if cmd.exe /c "echo test" > /dev/null 2>&1; then
    echo "✅ Can execute Windows commands from WSL"
else
    echo "❌ Cannot execute Windows commands"
fi

# Check if MCP server is accessible
echo ""
echo "Checking MCP server..."
if [ -d "/mnt/c/Devops/MCP/UE_Semantic_Analysis_MCP_Server" ]; then
    echo "✅ MCP server directory accessible"
else
    echo "❌ MCP server directory not found"
fi

# Test hook functionality
echo ""
echo "Testing hook environment variables..."
export CLAUDE_FILE_PATHS="/mnt/c/Devops/Projects/Polair/Source/test.cpp"
export CLAUDE_TOOL_INPUT='{"test": "data"}'
export CLAUDE_SESSION_ID="test_session_123"
export CLAUDE_PROMPT="Test prompt"

echo "✅ Environment variables set for hooks"

echo ""
echo "=== Setup Complete ==="
echo ""
echo "To use hooks in Claude Code:"
echo "1. The hooks will automatically trigger based on your actions"
echo "2. Use /hooks command to view active hooks"
echo "3. Use slash commands like /validate-epic or /review-build"
echo "4. Hooks will create logs in .claude/logs/"
echo ""
echo "Quick test:"
echo "  - Edit any .cpp file to trigger compilation hook"
echo "  - Modify phase docs to trigger update notification"
echo "  - Complete a session to trigger state update"