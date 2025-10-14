# Claude code
to run cluade code from docker file run the following command in terminal
```bash
  # from start 
  docker-compose build
  docker-compose up -d
  
  # to run claude code from docker compose
  docker-compose exec node claude
  
  # to run claude code
  docker exec -it nodejs_clion claude
```