#!/usr/bin/env python3
"""
Download GitHub Actions artifacts and rename them with version numbers.

Usage:
    python download_github_runs.py 2.0.4 https://github.com/78/xiaozhi-esp32/actions/runs/18866246016
"""

import argparse
import os
import re
import sys
import zipfile
from pathlib import Path
from urllib.parse import urlparse

import requests
from dotenv import load_dotenv


def parse_github_run_url(url: str) -> tuple[str, str, str]:
    """
    Parse GitHub Actions run URL to extract owner, repo, and run_id.
    
    Args:
        url: GitHub Actions run URL
        
    Returns:
        Tuple of (owner, repo, run_id)
    """
    # Example: https://github.com/78/xiaozhi-esp32/actions/runs/18866246016
    pattern = r'github\.com/([^/]+)/([^/]+)/actions/runs/(\d+)'
    match = re.search(pattern, url)
    
    if not match:
        raise ValueError(f"Invalid GitHub Actions URL: {url}")
    
    owner, repo, run_id = match.groups()
    return owner, repo, run_id


def get_artifacts(owner: str, repo: str, run_id: str, token: str) -> list[dict]:
    """
    Get all artifacts for a specific workflow run (with pagination support).
    
    Args:
        owner: Repository owner
        repo: Repository name
        run_id: Workflow run ID
        token: GitHub personal access token
        
    Returns:
        List of artifact dictionaries
    """
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28"
    }
    
    all_artifacts = []
    page = 1
    per_page = 100  # Maximum allowed by GitHub API
    
    while True:
        url = f"https://api.github.com/repos/{owner}/{repo}/actions/runs/{run_id}/artifacts"
        params = {
            "page": page,
            "per_page": per_page
        }
        
        response = requests.get(url, headers=headers, params=params)
        response.raise_for_status()
        
        data = response.json()
        artifacts = data.get("artifacts", [])
        
        if not artifacts:
            break
        
        all_artifacts.extend(artifacts)
        
        # Check if there are more pages
        total_count = data.get("total_count", 0)
        if len(all_artifacts) >= total_count:
            break
        
        page += 1
    
    return all_artifacts


def download_artifact(artifact_url: str, token: str, output_path: Path) -> None:
    """
    Download an artifact from GitHub.
    
    Args:
        artifact_url: Artifact download URL
        token: GitHub personal access token
        output_path: Path to save the downloaded artifact
    """
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28"
    }
    
    response = requests.get(artifact_url, headers=headers, stream=True)
    response.raise_for_status()
    
    # Create parent directory if it doesn't exist
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Download the file
    with open(output_path, 'wb') as f:
        for chunk in response.iter_content(chunk_size=8192):
            if chunk:
                f.write(chunk)


def rename_artifact(original_name: str, version: str) -> str:
    """
    Rename artifact according to the specified rules.
    
    Rules:
    - Remove "xiaozhi_" prefix
    - Remove hash suffix (underscore followed by hex string)
    - Add version prefix (e.g., "v2.0.4_")
    - Change extension to .zip
    
    Example:
        xiaozhi_atk-dnesp32s3-box0_43ef2f4e7f0957dc62ec7d628ac2819d226127b8.bin
        -> v2.0.4_atk-dnesp32s3-box0.zip
    
    Args:
        original_name: Original artifact name
        version: Version string (e.g., "2.0.4")
        
    Returns:
        New filename
    """
    # Remove "xiaozhi_" prefix
    name = original_name
    if name.startswith("xiaozhi_"):
        name = name[len("xiaozhi_"):]
    
    # Remove extension
    name_without_ext = os.path.splitext(name)[0]
    
    # Remove hash suffix (pattern: underscore followed by 40+ hex characters)
    # This matches Git commit hashes and similar identifiers
    name_without_hash = re.sub(r'_[a-f0-9]{40,}$', '', name_without_ext)
    
    # Add version prefix and .zip extension
    new_name = f"v{version}_{name_without_hash}.zip"
    
    return new_name


def main():
    """Main function to download and rename GitHub Actions artifacts."""
    parser = argparse.ArgumentParser(
        description="Download GitHub Actions artifacts and rename them with version numbers."
    )
    parser.add_argument(
        "version",
        help="Version number (e.g., 2.0.4)"
    )
    parser.add_argument(
        "url",
        help="GitHub Actions run URL (e.g., https://github.com/owner/repo/actions/runs/12345)"
    )
    parser.add_argument(
        "--output-dir",
        default="../releases",
        help="Output directory for downloaded artifacts (default: ../releases)"
    )
    
    args = parser.parse_args()
    
    # Load GitHub token from .env file
    load_dotenv()
    github_token = os.getenv("GITHUB_TOKEN")
    
    if not github_token:
        print("Error: GITHUB_TOKEN not found in environment variables.", file=sys.stderr)
        print("Please create a .env file with GITHUB_TOKEN=your_token_here", file=sys.stderr)
        sys.exit(1)
    
    try:
        # Parse the GitHub URL
        owner, repo, run_id = parse_github_run_url(args.url)
        print(f"Repository: {owner}/{repo}")
        print(f"Run ID: {run_id}")
        print(f"Version: {args.version}")
        print()
        
        # Get artifacts
        print("Fetching artifacts...")
        artifacts = get_artifacts(owner, repo, run_id, github_token)
        
        if not artifacts:
            print("No artifacts found for this run.")
            return
        
        print(f"Found {len(artifacts)} artifact(s):")
        for artifact in artifacts:
            print(f"  - {artifact['name']}")
        print()
        
        # Create output directory
        output_dir = Path(args.output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Download and rename each artifact
        downloaded_count = 0
        skipped_count = 0
        
        for artifact in artifacts:
            original_name = artifact['name']
            new_name = rename_artifact(original_name, args.version)
            final_path = output_dir / new_name
            
            # Check if file already exists
            if final_path.exists():
                print(f"Skipping (already exists): {original_name}")
                print(f"  -> {new_name}")
                print(f"  File: {final_path}")
                print()
                skipped_count += 1
                continue
            
            print(f"Downloading: {original_name}")
            print(f"  -> {new_name}")
            
            # Download to temporary path first
            temp_path = output_dir / f"{original_name}.zip"
            download_artifact(
                artifact['archive_download_url'],
                github_token,
                temp_path
            )
            
            # Rename to final name
            temp_path.rename(final_path)
            
            print(f"  Saved to: {final_path}")
            print()
            downloaded_count += 1
        
        print(f"Summary:")
        print(f"  Downloaded: {downloaded_count} artifact(s)")
        print(f"  Skipped: {skipped_count} artifact(s)")
        print(f"  Total: {len(artifacts)} artifact(s)")
        print(f"  Output directory: {output_dir.absolute()}")
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

