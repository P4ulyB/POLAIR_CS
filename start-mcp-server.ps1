# Start UE Semantic Analysis MCP Server
# Automates the startup sequence for the UE Semantic Analysis MCP Server
# Based on manual commands from PowerShell session

param(
    [switch]$Build,
    [switch]$Help
)

# Script configuration
$MCPServerPath = "C:\Devops\MCP\UE_Semantic_Analysis_MCP_Server"
$ScriptName = "start-mcp-server.ps1"

function Show-Help {
    Write-Host "UE Semantic Analysis MCP Server Startup Script" -ForegroundColor Cyan
    Write-Host "=============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "USAGE:" -ForegroundColor Yellow
    Write-Host "  .\$ScriptName              # Start server (default)"
    Write-Host "  .\$ScriptName -Build       # Build then start server"
    Write-Host "  .\$ScriptName -Help        # Show this help"
    Write-Host ""
    Write-Host "DESCRIPTION:" -ForegroundColor Yellow
    Write-Host "  Automates the startup sequence for UE Semantic Analysis MCP Server:"
    Write-Host "  1. Navigate to MCP server directory"
    Write-Host "  2. Execute npm start (or npm run build if -Build specified)"
    Write-Host "  3. Start the server with proper initialization"
    Write-Host ""
    Write-Host "SERVER PATH:" -ForegroundColor Yellow
    Write-Host "  $MCPServerPath"
    Write-Host ""
    Write-Host "EXPECTED OUTPUT:" -ForegroundColor Yellow
    Write-Host "  - UE Semantic MCP initialization"
    Write-Host "  - Search strategies initialization (Hybrid, Clangd, FTS, Basic)"
    Write-Host "  - PA Policy Engine with 22-point framework"
    Write-Host "  - Epic source patterns loading"
    Write-Host "  - Server ready for PA agents"
    Write-Host ""
    exit 0
}

function Test-MCPServerPath {
    if (-not (Test-Path $MCPServerPath)) {
        Write-Host "ERROR: MCP Server directory not found" -ForegroundColor Red
        Write-Host "Expected path: $MCPServerPath" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please verify the MCP server is installed at the expected location." -ForegroundColor Yellow
        exit 1
    }
}

function Test-NodeModules {
    $nodeModulesPath = Join-Path $MCPServerPath "node_modules"
    if (-not (Test-Path $nodeModulesPath)) {
        Write-Host "WARNING: node_modules directory not found" -ForegroundColor Yellow
        Write-Host "Running npm install first..." -ForegroundColor Yellow
        Write-Host ""
        
        Set-Location $MCPServerPath
        Write-Host "Executing: npm install" -ForegroundColor Cyan
        npm install
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: npm install failed" -ForegroundColor Red
            exit 1
        }
    }
}

function Start-MCPServer {
    param([bool]$BuildFirst = $false)
    
    Write-Host "UE Semantic Analysis MCP Server Startup" -ForegroundColor Green
    Write-Host "=======================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Server Path: $MCPServerPath" -ForegroundColor Cyan
    Write-Host "Build First: $BuildFirst" -ForegroundColor Cyan
    Write-Host ""
    
    # Navigate to MCP server directory
    Write-Host "Navigating to MCP server directory..." -ForegroundColor Yellow
    Set-Location $MCPServerPath
    
    if ($BuildFirst) {
        Write-Host ""
        Write-Host "Building MCP server..." -ForegroundColor Yellow
        Write-Host "Executing: npm run build" -ForegroundColor Cyan
        npm run build
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Build failed" -ForegroundColor Red
            exit 1
        }
        
        Write-Host ""
        Write-Host "Build completed successfully" -ForegroundColor Green
    }
    
    # Start the server
    Write-Host ""
    Write-Host "Starting UE Semantic Analysis MCP Server..." -ForegroundColor Yellow
    Write-Host "Executing: npm start" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Expected initialization sequence:" -ForegroundColor Gray
    Write-Host "  > ue-semantic-mcp@1.0.0 prestart" -ForegroundColor Gray
    Write-Host "  > npm run build" -ForegroundColor Gray
    Write-Host "  > ue-semantic-mcp@1.0.0 build" -ForegroundColor Gray
    Write-Host "  > tsc" -ForegroundColor Gray
    Write-Host "  > ue-semantic-mcp@1.0.0 start" -ForegroundColor Gray
    Write-Host "  > node dist/server.js" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Starting server..." -ForegroundColor Green
    Write-Host "Press Ctrl+C to stop the server" -ForegroundColor Yellow
    Write-Host ""
    
    # Execute npm start
    npm start
}

# Main execution
if ($Help) {
    Show-Help
}

Write-Host ""
Write-Host "UE Semantic Analysis MCP Server Startup Script" -ForegroundColor Green
Write-Host "===============================================" -ForegroundColor Green

# Validation checks
Test-MCPServerPath
Test-NodeModules

# Start the server
Start-MCPServer -BuildFirst:$Build

Write-Host ""
Write-Host "MCP Server startup script completed" -ForegroundColor Green